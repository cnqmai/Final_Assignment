#include "appointment_structures.h"
#include <iostream>
#include <ctime>
#include <limits>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#define NOMINMAX
#include <windows.h>
#define VIETNAM_TZ_OFFSET 7 * 3600

using namespace std;

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == string::npos || last == string::npos) return "";
    return str.substr(first, last - first + 1);
}

string toVietnamTime(time_t utc_time) {
    if (utc_time == 0) return "Chưa đặt thời gian";

    time_t vn_time = utc_time + VIETNAM_TZ_OFFSET;
    struct tm timeinfo;
    localtime_s(&timeinfo, &vn_time);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M", &timeinfo);
    return string(buffer);
}

bool isValidDate(int day, int month, int year) {
    if (year < 1970 || month < 1 || month > 12 || day < 1) return false;

    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        daysInMonth[1] = 29;
    }
    return day <= daysInMonth[month - 1];
}

time_t parseDateTime(const string& datetime) {
    string cleaned = trim(datetime);
    if (cleaned.empty()) {
        throw runtime_error("Chuỗi thời gian rỗng");
    }

    stringstream ss(cleaned);
    int day, month, year, hour, minute;
    char dash1, dash2, colon;

    ss >> day >> dash1 >> month >> dash2 >> year;

    if (ss.get() != ' ') {
        throw runtime_error("Dấu phân tách giữa ngày và giờ phải là khoảng trắng");
    }

    ss >> hour >> colon >> minute;

    if (ss.fail()) {
        throw runtime_error("Không thể phân tích thời gian. Đảm bảo định dạng DD-MM-YYYY HH:MM");
    }

    if (dash1 != '-' || dash2 != '-') {
        throw runtime_error("Dấu phân tách ngày/tháng/năm phải là '-'");
    }
    if (colon != ':') {
        throw runtime_error("Dấu phân tách giờ/phút phải là ':'");
    }

    string extra;
    getline(ss, extra);
    if (!trim(extra).empty()) {
        throw runtime_error("Có ký tự thừa sau thời gian");
    }

    if (!isValidDate(day, month, year)) {
        throw runtime_error("Ngày không hợp lệ (ngày 1-31, tháng 1-12, năm >= 1970)");
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        throw runtime_error("Giờ/phút không hợp lệ (giờ 0-23, phút 0-59)");
    }

    struct tm timeinfo = {};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;

    time_t result = mktime(&timeinfo);
    if (result == -1) {
        throw runtime_error("Không thể chuyển đổi thời gian");
    }

    result -= VIETNAM_TZ_OFFSET;
    return result;
}

time_t getCurrentTime() {
    return time(nullptr);
}

bool isAlphanumeric(const string& str) {
    if (str.empty()) return false;
    for (char c : str) {
        if (!isalnum(c)) return false;
    }
    return true;
}

class AppointmentSystem {
private:
    Hashmap<shared_ptr<Appointment>> appointments;
    Hashmap<vector<shared_ptr<Appointment>>> doctor_schedules; // Lưu lịch hẹn theo bác sĩ
    Hashmap<vector<shared_ptr<Appointment>>> patient_schedules; // Lưu lịch hẹn theo bệnh nhân
    AVLTree schedule;
    PriorityQueue reminders;
    DoublyLinkedList doctor_appointments;

public:
    ~AppointmentSystem() {}

    bool KiemTraThoiGianTrong(const string& did, time_t time) {
        auto* schedule = doctor_schedules.Find(did);
        if (!schedule) return true;
        for (const auto& app : *schedule) {
            if (app->is_valid && abs(difftime(time, app->time)) < 1800) { // Trùng trong 30 phút
                return false;
            }
        }
        return true;
    }

    void ThemLichHen(const string& aid, const string& pid, const string& did, time_t time, const string& status) {
        if (!KiemTraThoiGianTrong(did, time)) {
            throw runtime_error("Bác sĩ không trống tại thời gian này");
        }
        auto sp = make_shared<Appointment>(aid, pid, did, time, status);
        try {
            appointments.Insert(aid, sp);
            try {
                // Thêm vào lịch bác sĩ
                auto* doc_schedule = doctor_schedules.Find(did);
                if (!doc_schedule) {
                    vector<shared_ptr<Appointment>> new_schedule;
                    new_schedule.push_back(sp);
                    doctor_schedules.Insert(did, new_schedule);
                }
                else {
                    doc_schedule->push_back(sp);
                }
                // Thêm vào lịch bệnh nhân
                auto* pat_schedule = patient_schedules.Find(pid);
                if (!pat_schedule) {
                    vector<shared_ptr<Appointment>> new_schedule;
                    new_schedule.push_back(sp);
                    patient_schedules.Insert(pid, new_schedule);
                }
                else {
                    pat_schedule->push_back(sp);
                }
                // Thêm vào các cấu trúc khác
                schedule.Insert(sp);
                reminders.Push(sp);
                doctor_appointments.Append(sp);
                cout << "Đã thêm lịch hẹn " << aid << " thành công." << endl;
            }
            catch (...) {
                appointments.Remove(aid);
                throw;
            }
        }
        catch (...) {
            throw;
        }
    }

    void XoaLichHen(const string& aid, const string& user_id, bool is_doctor) {
        shared_ptr<Appointment>* app = appointments.Find(aid);
        if (!app || !(*app)) throw runtime_error("Không tìm thấy lịch hẹn");
        if (is_doctor && (*app)->doctor_id != user_id) {
            throw runtime_error("Bạn không phải bác sĩ của lịch hẹn này");
        }
        if (!is_doctor && (*app)->patient_id != user_id) {
            throw runtime_error("Bạn không phải bệnh nhân của lịch hẹn này");
        }
        (*app)->is_valid = false;
        appointments.Remove(aid);
        schedule.Remove(aid);
        doctor_appointments.Remove(aid);
        cout << "Đã xóa lịch hẹn " << aid << " thành công." << endl;
    }

    void ChinhSuaLichHen(const string& aid, time_t new_time, const string& new_doctor_id) {
        shared_ptr<Appointment>* app = appointments.Find(aid);
        if (!app || !(*app)) throw runtime_error("Không tìm thấy lịch hẹn");
        if (!KiemTraThoiGianTrong(new_doctor_id, new_time)) {
            throw runtime_error("Bác sĩ mới không trống tại thời gian này");
        }
        (*app)->is_valid = false;
        schedule.Remove(aid);
        doctor_appointments.Remove(aid);
        // Cập nhật lịch bác sĩ
        auto* old_doc_schedule = doctor_schedules.Find((*app)->doctor_id);
        if (old_doc_schedule) {
            old_doc_schedule->erase(
                remove_if(old_doc_schedule->begin(), old_doc_schedule->end(),
                    [&](const shared_ptr<Appointment>& a) { return a->appointment_id == aid; }),
                old_doc_schedule->end());
        }
        auto* new_doc_schedule = doctor_schedules.Find(new_doctor_id);
        if (!new_doc_schedule) {
            vector<shared_ptr<Appointment>> new_schedule;
            new_schedule.push_back(*app);
            doctor_schedules.Insert(new_doctor_id, new_schedule);
        }
        else {
            new_doc_schedule->push_back(*app);
        }
        // Cập nhật thông tin lịch hẹn
        (*app)->time = new_time;
        (*app)->doctor_id = new_doctor_id;
        (*app)->is_valid = true;
        schedule.Insert(*app);
        reminders.Push(*app);
        doctor_appointments.Append(*app);
        cout << "Đã chỉnh sửa lịch hẹn " << aid << " thành công." << endl;
    }

    void XacNhanLichHen(const string& aid, const string& doctor_id, bool confirm) {
        shared_ptr<Appointment>* app = appointments.Find(aid);
        if (!app || !(*app)) throw runtime_error("Không tìm thấy lịch hẹn");
        if ((*app)->doctor_id != doctor_id) {
            throw runtime_error("Bạn không phải bác sĩ của lịch hẹn này");
        }
        if ((*app)->status == "bị từ chối") {
            throw runtime_error("Lịch hẹn đã bị từ chối trước đó");
        }
        (*app)->status = confirm ? "đã xác nhận" : "bị từ chối";
        (*app)->is_valid = confirm;
        cout << "Lịch hẹn " << aid << " đã được " << (confirm ? "xác nhận" : "từ chối") << "." << endl;
    }

    shared_ptr<Appointment> TimLichHen(const string& aid) {
        shared_ptr<Appointment>* app = appointments.Find(aid);
        if (!app || !(*app) || !(*app)->is_valid || (*app)->appointment_id != aid) {
            cout << "Không tìm thấy lịch hẹn " << aid << "." << endl;
            return nullptr;
        }
        return *app;
    }

    void TimLichHenTheoBenhNhan(const string& pid) {
        auto* schedule = patient_schedules.Find(pid);
        if (!schedule || schedule->empty()) {
            cout << "Không tìm thấy lịch hẹn nào cho bệnh nhân " << pid << "." << endl;
            return;
        }
        for (const auto& app : *schedule) {
            if (app->is_valid) {
                cout << "Lịch hẹn " << app->appointment_id
                    << " với bác sĩ " << app->doctor_id
                    << " vào lúc " << toVietnamTime(app->time)
                    << ", trạng thái: " << app->status << endl;
            }
        }
    }

    void TimLichHenTheoBacSi(const string& did) {
        auto result = doctor_appointments.FindByDoctor(did);
        if (result.empty()) {
            cout << "Không tìm thấy lịch hẹn nào cho bác sĩ " << did << "." << endl;
            return;
        }
        for (const auto& app : result) {
            cout << "Lịch hẹn " << app->appointment_id
                << " với bệnh nhân " << app->patient_id
                << " vào lúc " << toVietnamTime(app->time)
                << ", trạng thái: " << app->status << endl;
        }
    }

    void TimLichHenTheoThoiGian(const string& start_datetime, const string& end_datetime) {
        time_t start = parseDateTime(start_datetime);
        time_t end = parseDateTime(end_datetime);
        if (difftime(end, start) < 0) {
            throw runtime_error("Thời gian kết thúc phải sau thời gian bắt đầu");
        }
        auto result = schedule.FindByTimeRange(start, end);
        if (result.empty()) {
            cout << "Không tìm thấy lịch hẹn nào trong khoảng thời gian từ "
                << toVietnamTime(start) << " đến " << toVietnamTime(end) << "." << endl;
            return;
        }
        for (const auto& app : result) {
            cout << "Lịch hẹn " << app->appointment_id
                << " với bệnh nhân " << app->patient_id
                << ", bác sĩ " << app->doctor_id
                << " vào lúc " << toVietnamTime(app->time)
                << ", trạng thái: " << app->status << endl;
        }
    }

    void LietKeLichHenTrongNgay() {
        time_t now = getCurrentTime();
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        timeinfo.tm_hour = 0;
        timeinfo.tm_min = 0;
        timeinfo.tm_sec = 0;
        time_t start = mktime(&timeinfo) - VIETNAM_TZ_OFFSET;
        time_t end = start + 86400; // 1 ngày
        auto result = schedule.FindByTimeRange(start, end);
        if (result.empty()) {
            cout << "Không có lịch hẹn nào trong ngày hôm nay (" << toVietnamTime(start) << ")." << endl;
            return;
        }
        cout << "Lịch hẹn trong ngày hôm nay (" << toVietnamTime(start) << "):" << endl;
        for (const auto& app : result) {
            cout << "Lịch hẹn " << app->appointment_id
                << " với bệnh nhân " << app->patient_id
                << ", bác sĩ " << app->doctor_id
                << " vào lúc " << toVietnamTime(app->time)
                << ", trạng thái: " << app->status << endl;
        }
    }

    void GuiNhacNho(int hours_before) {
        bool has_reminders = false;
        vector<shared_ptr<Appointment>> temp_reminders;

        while (!reminders.IsEmpty()) {
            auto app = reminders.Pop();
            if (app && app->is_valid) {
                temp_reminders.push_back(app);
            }
        }

        sort(temp_reminders.begin(), temp_reminders.end(),
            [](const shared_ptr<Appointment>& a, const shared_ptr<Appointment>& b) {
                return a->time < b->time;
            });

        time_t now = getCurrentTime();
        time_t threshold = hours_before * 3600;

        for (const auto& app : temp_reminders) {
            time_t appointmentTime = app->time + VIETNAM_TZ_OFFSET;
            if (appointmentTime > now && difftime(appointmentTime, now) <= threshold) {
                cout << "Nhắc nhở: Lịch hẹn " << app->appointment_id
                    << " với bệnh nhân " << app->patient_id
                    << ", bác sĩ " << app->doctor_id
                    << " vào lúc " << toVietnamTime(app->time)
                    << ", trạng thái: " << app->status << endl;
                has_reminders = true;
            }
        }

        for (const auto& app : temp_reminders) {
            reminders.Push(app);
        }

        if (!has_reminders) {
            cout << "Không có lịch hẹn nào cần nhắc nhở trong " << hours_before << " giờ tới." << endl;
        }
    }

    bool KiemTraIDTonTai(const string& aid) {
        shared_ptr<Appointment>* app = appointments.Find(aid);
        return app && *app && (*app)->is_valid;
    }
};

void clearInputBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    AppointmentSystem system;
    int choice;

    do {
        cout << "\n=== HỆ THỐNG ĐẶT LỊCH HẸN BÁC SĨ ===\n";
        cout << "1. Thêm lịch hẹn\n";
        cout << "2. Xóa lịch hẹn\n";
        cout << "3. Chỉnh sửa lịch hẹn\n";
        cout << "4. Tìm kiếm lịch hẹn theo ID\n";
        cout << "5. Tìm kiếm lịch hẹn theo bệnh nhân\n";
        cout << "6. Tìm kiếm lịch hẹn theo bác sĩ\n";
        cout << "7. Tìm kiếm lịch hẹn theo thời gian\n";
        cout << "8. Xác nhận/từ chối lịch hẹn\n";
        cout << "9. Gửi nhắc nhở\n";
        cout << "10. Liệt kê lịch hẹn trong ngày\n";
        cout << "11. Thoát\n";
        cout << "Nhập lựa chọn (1-11): ";
        cin >> choice;
        clearInputBuffer();

        try {
            switch (choice) {
            case 1: {
                string aid, pid, did, status, datetime;
                time_t appointment_time;

                while (true) {
                    cout << "Nhập ID lịch hẹn: ";
                    getline(cin, aid);
                    if (!isAlphanumeric(aid)) {
                        cout << "Lỗi: ID chỉ được chứa chữ cái và số, không rỗng. Nhập lại.\n";
                        continue;
                    }
                    if (system.KiemTraIDTonTai(aid)) {
                        cout << "Lỗi: ID lịch hẹn đã tồn tại. Nhập lại.\n";
                        continue;
                    }
                    break;
                }

                while (true) {
                    cout << "Nhập ID bệnh nhân: ";
                    getline(cin, pid);
                    if (!isAlphanumeric(pid)) {
                        cout << "Lỗi: ID bệnh nhân chỉ được chứa chữ cái và số, không rỗng. Nhập lại.\n";
                        continue;
                    }
                    break;
                }

                while (true) {
                    cout << "Nhập ID bác sĩ: ";
                    getline(cin, did);
                    if (!isAlphanumeric(did)) {
                        cout << "Lỗi: ID bác sĩ chỉ được chứa chữ cái và số, không rỗng. Nhập lại.\n";
                        continue;
                    }
                    break;
                }

                while (true) {
                    cout << "Nhập thời gian (DD-MM-YYYY HH:MM, giờ Việt Nam): ";
                    getline(cin, datetime);
                    try {
                        appointment_time = parseDateTime(datetime);
                        if (difftime(appointment_time + VIETNAM_TZ_OFFSET, getCurrentTime()) <= 0) {
                            cout << "Lỗi: Không thể đặt lịch hẹn trong quá khứ hoặc hiện tại. Nhập lại.\n";
                            continue;
                        }
                        break;
                    }
                    catch (const runtime_error& e) {
                        cout << "Lỗi: " << e.what() << ". Nhập lại.\n";
                    }
                }

                while (true) {
                    cout << "Nhập trạng thái (đang chờ/đã xác nhận): ";
                    getline(cin, status);
                    if (status != "đang chờ" && status != "đã xác nhận") {
                        cout << "Lỗi: Trạng thái chỉ được là 'đang chờ' hoặc 'đã xác nhận'. Nhập lại.\n";
                        continue;
                    }
                    break;
                }

                system.ThemLichHen(aid, pid, did, appointment_time, status);
                break;
            }
            case 2: {
                string aid, user_id;
                bool is_doctor;
                cout << "Nhập ID lịch hẹn cần xóa: ";
                getline(cin, aid);
                while (true) {
                    cout << "Bạn là bác sĩ hay bệnh nhân? (0: bệnh nhân, 1: bác sĩ): ";
                    string role;
                    getline(cin, role);
                    if (role == "0") {
                        is_doctor = false;
                        break;
                    }
                    else if (role == "1") {
                        is_doctor = true;
                        break;
                    }
                    cout << "Lỗi: Vui lòng nhập 0 hoặc 1. Nhập lại.\n";
                }
                cout << "Nhập ID của bạn (bệnh nhân hoặc bác sĩ): ";
                getline(cin, user_id);
                system.XoaLichHen(aid, user_id, is_doctor);
                break;
            }
            case 3: {
                string aid, new_did, datetime;
                time_t new_time;

                while (true) {
                    cout << "Nhập ID lịch hẹn cần chỉnh sửa: ";
                    getline(cin, aid);
                    if (!isAlphanumeric(aid)) {
                        cout << "Lỗi: ID chỉ được chứa chữ cái và số, không rỗng. Nhập lại.\n";
                        continue;
                    }
                    if (!system.KiemTraIDTonTai(aid)) {
                        cout << "Lỗi: ID lịch hẹn không tồn tại. Nhập lại.\n";
                        continue;
                    }
                    break;
                }

                while (true) {
                    cout << "Nhập ID bác sĩ mới: ";
                    getline(cin, new_did);
                    if (!isAlphanumeric(new_did)) {
                        cout << "Lỗi: ID bác sĩ chỉ được chứa chữ cái và số, không rỗng. Nhập lại.\n";
                        continue;
                    }
                    break;
                }

                while (true) {
                    cout << "Nhập thời gian mới (DD-MM-YYYY HH:MM, giờ Việt Nam): ";
                    getline(cin, datetime);
                    try {
                        new_time = parseDateTime(datetime);
                        if (difftime(new_time + VIETNAM_TZ_OFFSET, getCurrentTime()) <= 0) {
                            cout << "Lỗi: Không thể đặt lịch hẹn trong quá khứ hoặc hiện tại. Nhập lại.\n";
                            continue;
                        }
                        break;
                    }
                    catch (const runtime_error& e) {
                        cout << "Lỗi: " << e.what() << ". Nhập lại.\n";
                    }
                }

                system.ChinhSuaLichHen(aid, new_time, new_did);
                break;
            }
            case 4: {
                string aid;
                cout << "Nhập ID lịch hẹn cần tìm: ";
                getline(cin, aid);
                auto app = system.TimLichHen(aid);
                if (app) {
                    cout << "Tìm thấy: Lịch hẹn " << app->appointment_id
                        << " với bác sĩ " << app->doctor_id
                        << " vào lúc " << toVietnamTime(app->time)
                        << ", trạng thái: " << app->status << endl;
                }
                break;
            }
            case 5: {
                string pid;
                cout << "Nhập ID bệnh nhân: ";
                getline(cin, pid);
                system.TimLichHenTheoBenhNhan(pid);
                break;
            }
            case 6: {
                string did;
                cout << "Nhập ID bác sĩ: ";
                getline(cin, did);
                system.TimLichHenTheoBacSi(did);
                break;
            }
            case 7: {
                string start_datetime, end_datetime;
                cout << "Nhập thời gian bắt đầu (DD-MM-YYYY HH:MM): ";
                getline(cin, start_datetime);
                cout << "Nhập thời gian kết thúc (DD-MM-YYYY HH:MM): ";
                getline(cin, end_datetime);
                system.TimLichHenTheoThoiGian(start_datetime, end_datetime);
                break;
            }
            case 8: {
                string aid, doctor_id;
                bool confirm;
                cout << "Nhập ID lịch hẹn cần xác nhận/từ chối: ";
                getline(cin, aid);
                cout << "Nhập ID bác sĩ: ";
                getline(cin, doctor_id);
                while (true) {
                    cout << "Xác nhận lịch hẹn? (0: từ chối, 1: xác nhận): ";
                    string input;
                    getline(cin, input);
                    if (input == "0") {
                        confirm = false;
                        break;
                    }
                    else if (input == "1") {
                        confirm = true;
                        break;
                    }
                    cout << "Lỗi: Vui lòng nhập 0 hoặc 1. Nhập lại.\n";
                }
                system.XacNhanLichHen(aid, doctor_id, confirm);
                break;
            }
            case 9: {
                int hours_before;
                while (true) {
                    cout << "Nhập khoảng thời gian nhắc nhở (1: 1 giờ, 24: 1 ngày): ";
                    cin >> hours_before;
                    clearInputBuffer();
                    if (hours_before == 1 || hours_before == 24) break;
                    cout << "Lỗi: Vui lòng nhập 1 hoặc 24. Nhập lại.\n";
                }
                system.GuiNhacNho(hours_before);
                break;
            }
            case 10: {
                system.LietKeLichHenTrongNgay();
                break;
            }
            case 11: {
                cout << "Đang thoát chương trình...\n";
                break;
            }
            default: {
                cout << "Lựa chọn không hợp lệ. Vui lòng chọn lại.\n";
                break;
            }
            }
        }
        catch (const runtime_error& e) {
            cerr << "Lỗi: " << e.what() << endl;
        }
    } while (choice != 11);

    return 0;
}