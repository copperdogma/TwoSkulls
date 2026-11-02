#include "skull_fault_log_core.h"
#include "skull_fault_archive.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace skull::faultlog;

namespace {
std::string read_file(const std::string &path) {
    FILE *file = std::fopen(path.c_str(), "rb");
    assert(file);
    std::string result;
    char buffer[512];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
        result.append(buffer, n);
    assert(!std::ferror(file));
    assert(std::fclose(file) == 0);
    return result;
}

void write_file(const std::string &path, const char *text) {
    FILE *file = std::fopen(path.c_str(), "wb");
    assert(file);
    const size_t length = std::strlen(text);
    assert(std::fwrite(text, 1, length, file) == length);
    assert(std::fclose(file) == 0);
}

void remove_tree(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (!dir) { unlink(path.c_str()); return; }
    while (const dirent *entry = readdir(dir)) {
        if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
        const std::string child = path + "/" + entry->d_name;
        struct stat info{};
        if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) remove_tree(child);
        else unlink(child.c_str());
    }
    closedir(dir);
    rmdir(path.c_str());
}

std::string temp_directory() {
    char path[] = "/tmp/skull-fault-log-test-XXXXXX";
    char *created = mkdtemp(path);
    assert(created);
    return created;
}

void test_ring_freeze_and_coalescing() {
    Core core;
    core.session = 0x1234;
    std::strcpy(core.firmware, "test-build");
    for (size_t i = 0; i < ring_lines + 3; ++i) {
        char line[64];
        std::snprintf(line, sizeof(line), "I (100) anthony: event-%02u", unsigned(i));
        core.capture(line, false, static_cast<int64_t>(i) * 1000);
    }
    assert(core.count == ring_lines);
    core.capture("E (900) anthony: first failure", true, 900000);
    assert(core.pending);
    core.materialize();core.frozen=false;
    assert(core.report.id == ((uint64_t(0x1234) << 32) | 1));
    assert(core.report.uptime_us == 900000);
    const std::string frozen(core.report.text, core.report.length);
    assert(frozen.find("event-00") == std::string::npos);
    // The fault line itself enters the ring before snapshotting and evicts event-03.
    assert(frozen.find("event-03") == std::string::npos);
    assert(frozen.find("event-04") != std::string::npos);
    assert(frozen.find("event-34") != std::string::npos);
    core.capture("E (901) skull_sd: another failure", true, 901000);
    core.capture("E (902) skull_radio: later failure", true, 902000);
    assert(core.coalesced == 2);
    assert(std::string(core.report.text, core.report.length) == frozen);
    core.acknowledged();
    assert(!core.pending);
}

void test_selection_and_bounds() {
    char output[line_bytes]{};
    bool fault = false;
    assert(!select_line("I (1) CHIP: provisioning secret", output, sizeof(output), fault));
    assert(!select_line("E (2) wifi: password=secret", output, sizeof(output), fault));
    assert(!select_line("I (2) skull_sd: Card config role=primary speaker='private'", output, sizeof(output), fault));
    assert(!select_line("E (2) anthony: password=secret", output, sizeof(output), fault));
    assert(select_line("\033[0;31mE (3) anthony: audio read failed\033[0m\nnext", output,
                       sizeof(output), fault));
    assert(fault);
    assert(std::strstr(output, "\033") == nullptr);
    assert(std::strstr(output, "audio read failed") != nullptr);
    assert(select_line("E (4) sdspi_host: data CRC mismatch", output, sizeof(output), fault));
    assert(fault);
    assert(select_line("W (4) skull_show: Trigger consumed: busy/cooldown/speaker/peer unavailable", output, sizeof(output), fault));
    assert(!fault);
    assert(select_line("W (5) skull_show: Playback canceled: speaker unavailable or SD/PCM failure; no automatic replay", output, sizeof(output), fault));
    assert(fault);

    assert(select_line("W (5) skull_startup: Startup announcement interrupted by link loss; no automatic replay", output, sizeof(output), fault) && fault);
    assert(select_line("W (5) skull_radio: Jeff setup timed out", output, sizeof(output), fault) && fault);
    assert(select_line("W (5) skull_radio: Speaker unavailable; retry later", output, sizeof(output), fault) && !fault);

    for (const char *tag : {"skull", "anthony", "jeff", "skull_show", "skull_radio", "skull_startup", "skull_sd"}) {
        char line[160];
        std::snprintf(line, sizeof(line), "E (6) %s: error for either role", tag);
        assert(select_line(line, output, sizeof(output), fault) && fault);
    }
    assert(select_line("I (6) lifecycle: event=wifi_sample rssi_dbm=-65", output, sizeof(output), fault) && !fault);
    assert(!select_line("I (6) lifecycle: event=wifi_sample ssid=private", output, sizeof(output), fault));
    assert(!select_line("E (7) faultlog: SD archive deferred", output, sizeof(output), fault));
    assert(!select_line("E (8) CHIP: setup_code=secret", output, sizeof(output), fault));

    Core core;
    std::vector<char> huge(20000, 'X');
    huge.back() = '\0';
    core.capture(huge.data(), false, 1);
    assert(std::strlen(core.recent[0]) == line_bytes - 1);
    core.capture(nullptr, true, 2);
    assert(!core.pending);
}

void test_lifecycle_survives_noise_and_freezes() {
    Core core;
    core.capture("I (10) lifecycle: event=boot reset_reason=poweron", false, 10000);
    core.capture("I (20) lifecycle: event=sd_mount_result result=FAIL duration_ms=15", false, 20000);
    core.capture("I (30) lifecycle: event=sd_mount_result result=OK duration_ms=2", false, 30000);
    core.capture("I (31) lifecycle: event=speaker_disconnected count=1", false, 31000);
    core.capture("I (32) lifecycle: event=speaker_rssi rssi_delta=-4", false, 32000);
    for (unsigned i=0; i<100; ++i) {
        char line[160];
        std::snprintf(line,sizeof(line),"I (%u) lifecycle: event=wifi_sample rssi_dbm=-%u",i+40,i);
        core.capture(line,false,40000+i);
        core.capture("I (200) anthony: STATUS routine noise",false,200000);
    }
    assert(!core.pending);
    core.capture("E (300) sdmmc_cmd: failure",true,300000);
    // Mount result is known after the low-level driver emits its first error.
    core.capture("I (301) lifecycle: event=sd_result result=FAIL duration_ms=500",false,301000);
    core.seal_context();
    core.capture("I (302) lifecycle: event=speaker_disconnected after_seal=1",false,302000);
    core.materialize();core.frozen=false;
    const std::string saved(core.report.text,core.report.length);
    assert(saved.find("event=boot reset_reason=poweron")!=std::string::npos);
    assert(saved.find("event=sd_mount_result result=FAIL duration_ms=15")!=std::string::npos);
    assert(saved.find("event=sd_mount_result result=OK duration_ms=2")!=std::string::npos);
    assert(saved.find("event=speaker_disconnected count=1")!=std::string::npos);
    assert(saved.find("event=speaker_rssi rssi_delta=-4")!=std::string::npos);
    assert(saved.find("event=sd_result result=FAIL duration_ms=500")!=std::string::npos);
    assert(saved.find("rssi_dbm=-99")!=std::string::npos);
    assert(saved.find("after_seal=1")==std::string::npos);
    assert(saved.find("sdmmc_cmd: failure")!=std::string::npos);
    core.capture("I (400) lifecycle: event=speaker_connected duration_ms=5",false,400000);
    assert(saved==std::string(core.report.text,core.report.length));
    core.acknowledged();core.capture("E (500) anthony: next fault",true,500000);
    core.materialize();core.frozen=false;
    assert(std::strstr(core.report.text,"after_seal=1"));
    assert(std::strstr(core.report.text,"event=boot reset_reason=poweron"));
}

void test_max_context_report_keeps_fault_tail() {
    Core core;
    char line[line_bytes];
    for(size_t i=0;i<Lifecycle::milestones;++i){
        std::snprintf(line,sizeof(line),"I (1) lifecycle: event=%s ",Lifecycle::key(i));
        const size_t n=std::strlen(line);std::memset(line+n,'m',sizeof(line)-n-1);line[sizeof(line)-1]=0;
        core.capture(line,false,1);
    }
    for(size_t i=0;i<Lifecycle::events;++i){
        std::snprintf(line,sizeof(line),"I (1) lifecycle: event=wifi_sample ");
        const size_t n=std::strlen(line);std::memset(line+n,'r',sizeof(line)-n-1);line[sizeof(line)-1]=0;
        core.capture(line,false,1);
    }
    std::memset(line,'x',sizeof(line)-1);line[sizeof(line)-1]=0;
    for(size_t i=0;i<ring_lines;++i)core.capture(line,false,1);
    core.capture("E (2) skull: FAULT_MUST_SURVIVE",true,2);core.materialize();
    assert(core.report.length<report_bytes);
    assert(std::strstr(core.report.text,"FAULT_MUST_SURVIVE"));
    assert(std::strstr(core.report.text,"event=speaker_stream_ready"));
}

void test_retry_cadence() {
    Cadence cadence;
    assert(!cadence.due(0, false, true));
    assert(!cadence.due(0, true, false));
    assert(cadence.due(0, true, true));
    cadence.attempted(0); // includes unavailable-card attempts
    assert(cadence.attempts == 1);
    assert(!cadence.due(retry_us - 1, true, true));
    assert(!cadence.due(retry_us, false, true));
    assert(cadence.due(retry_us, true, true)); // recovered card is eligible at cadence
    cadence.attempted(retry_us);
    assert(!cadence.due(retry_us + 1, true, true));
    assert(cadence.attempts == 2);
}

void test_archive_rotation_and_audio_preservation(const std::string &root) {
    const std::string audio = root + "/audio";
    const std::string archive = root + "/diagnostics";
    assert(mkdir(audio.c_str(), 0755) == 0);
    const std::string wav = audio + "/keep.wav";
    write_file(wav, "original audio bytes\0with tail");
    const std::string original_audio = read_file(wav);
    for (unsigned i = 0; i < archive_slots + 3; ++i) {
        Report report;
        const int n = std::snprintf(report.text, sizeof(report.text), "report-%u\n", i);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(report.text));
        report.length = static_cast<uint32_t>(n);
        assert(archive_report(archive.c_str(), i % archive_slots, report));
    }
    for (unsigned slot = 0; slot < archive_slots; ++slot) {
        const std::string contents = read_file(archive + "/fault-" + std::to_string(slot) + ".log");
        const unsigned expected = slot < 3 ? slot + archive_slots : slot;
        assert(contents == "report-" + std::to_string(expected) + "\n");
    }
    assert(read_file(wav) == original_audio);
    DIR *dir = opendir(archive.c_str());
    assert(dir);
    unsigned reports = 0;
    while (const dirent *entry = readdir(dir))
        if (!std::strncmp(entry->d_name, "fault-", 6)) ++reports;
    closedir(dir);
    assert(reports == archive_slots);
}

void test_archive_rejections_and_io_failures(const std::string &root) {
    Report report;
    const char payload[] = "frozen report\n";
    std::memcpy(report.text, payload, sizeof(payload));
    report.length = sizeof(payload) - 1;
    assert(!archive_report(nullptr, 0, report));
    assert(!archive_report(root.c_str(), archive_slots, report));
    report.length = report_bytes;
    assert(!archive_report(root.c_str(), 0, report));
    report.length = sizeof(payload) - 1;

    // The report staging open must fail cleanly when the requested parent is absent.
    const std::string absent = root + "/absent/child";
    assert(!archive_report(absent.c_str(), 0, report));

    // A directory at the fixed staging filename causes fopen("wb") to fail.
    const std::string blocked = root + "/blocked";
    assert(mkdir(blocked.c_str(), 0755) == 0);
    assert(mkdir((blocked + "/pending.tmp").c_str(), 0755) == 0);
    assert(!archive_report(blocked.c_str(), 0, report));
    assert(access((blocked + "/pending.tmp").c_str(), F_OK) == 0);

    // A directory in the target slot makes replacement fail after staging/readback;
    // the caller can retain and retry the same frozen report.
    const std::string target_blocked = root + "/target-blocked";
    assert(mkdir(target_blocked.c_str(), 0755) == 0);
    assert(mkdir((target_blocked + "/fault-0.log").c_str(), 0755) == 0);
    assert(!archive_report(target_blocked.c_str(), 0, report));
    assert(access((target_blocked + "/fault-0.log").c_str(), F_OK) == 0);
}

struct ArchiveCapture {
    std::string text;
    size_t calls=0;
    bool accept=true;
};
bool capture_archive(const uint8_t *bytes,size_t length,void *context) {
    auto &capture=*static_cast<ArchiveCapture *>(context);
    assert(length>0&&length<=64);
    ++capture.calls;
    if(!capture.accept)return false;
    capture.text.append(reinterpret_cast<const char *>(bytes),length);
    return true;
}
void test_read_saved_archive(const std::string &root) {
    const std::string directory=root+"/reader";
    assert(mkdir(directory.c_str(),0755)==0);
    const std::string unrelated=root+"/audio/keep.wav";
    const std::string original_audio=read_file(unrelated);
    Report report;
    report.length=report_bytes-1;
    for(size_t i=0;i<report.length;++i)report.text[i]=static_cast<char>(i%256);
    report.text[report.length]=0;
    assert(archive_report(directory.c_str(),2,report));
    const std::string original=read_file(directory+"/fault-2.log");
    ArchiveCapture capture;
    assert(read_archive(directory.c_str(),2,capture_archive,&capture));
    assert(capture.text==std::string(report.text,report.length));
    assert(capture.calls==(report.length+63)/64);
    assert(read_file(directory+"/fault-2.log")==original);
    assert(read_file(unrelated)==original_audio);

    ArchiveCapture invalid;
    assert(!read_archive(nullptr,2,capture_archive,&invalid));
    assert(errno==EINVAL);
    assert(!read_archive(directory.c_str(),archive_slots,capture_archive,&invalid));
    assert(!read_archive(directory.c_str(),2,nullptr,&invalid));
    assert(!read_archive(directory.c_str(),3,capture_archive,&invalid));
    assert(errno==ENOENT);
    assert(!read_archive(std::string(200,'x').c_str(),0,capture_archive,&invalid));
    assert(errno==ENAMETOOLONG);
    assert(invalid.calls==0);

    // The cap is enforced before handing any contents to the serial consumer.
    const std::string too_large(report_bytes,'x');
    write_file((directory+"/fault-3.log").c_str(),too_large.c_str());
    assert(!read_archive(directory.c_str(),3,capture_archive,&invalid));
    assert(errno==EFBIG&&invalid.calls==0);
    assert(mkdir((directory+"/fault-4.log").c_str(),0755)==0);
    assert(!read_archive(directory.c_str(),4,capture_archive,&invalid));
    assert(invalid.calls==0);

    ArchiveCapture declined;declined.accept=false;
    assert(!read_archive(directory.c_str(),2,capture_archive,&declined));
    assert(errno==ECANCELED&&declined.calls==1&&declined.text.empty());
    assert(read_file(directory+"/fault-2.log")==original);
    write_file((directory+"/fault-5.log").c_str(),"");
    assert(read_archive(directory.c_str(),5,capture_archive,&invalid));
    assert(invalid.calls==0); // Empty saved file is a successful zero-byte read.
}
} // namespace

int main() {
    const std::string root = temp_directory();
    test_ring_freeze_and_coalescing();
    test_selection_and_bounds();
    test_retry_cadence();
    test_lifecycle_survives_noise_and_freezes();
    test_max_context_report_keeps_fault_tail();
    test_archive_rotation_and_audio_preservation(root);
    test_archive_rejections_and_io_failures(root);
    test_read_saved_archive(root);
    remove_tree(root);
    std::puts("fault log core/archive tests passed");
    return 0;
}
