#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SiteResult {
    std::string site;
    std::string count;
    std::string status;
    std::string message;
    std::string sourceUrl;
};

struct Record {
    std::string timestampKst;
    std::string dateKst;
    std::string jobKoreaCount;
    std::string jobKoreaStatus;
    std::string jobKoreaMessage;
    std::string jobKoreaSource;
};

struct WeeklyCandle {
    std::string weekStart;
    int open = 0;
    int high = 0;
    int low = 0;
    int close = 0;
};

struct Options {
    std::filesystem::path outputDir;
    bool skipJobKorea = false;
};

const std::vector<std::string> kHeaders = {
    "TimestampKST",
    "DateKST",
    "JobKoreaCount",
    "JobKoreaStatus",
    "JobKoreaMessage",
    "JobKoreaSource",
};

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string HttpGet(const std::string& url, const std::wstring& acceptHeader = L"text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8") {
    const std::wstring wideUrl = ToWide(url);

    URL_COMPONENTSW parts{};
    wchar_t host[256]{};
    wchar_t path[4096]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &parts)) {
        throw std::runtime_error("Invalid URL: " + url);
    }

    std::wstring pathAndQuery(path, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength > 0) {
        pathAndQuery.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    if (pathAndQuery.empty()) {
        pathAndQuery = L"/";
    }

    HINTERNET session = WinHttpOpen(
        L"JobPostCounter/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) {
        throw std::runtime_error("WinHttpOpen failed.");
    }

    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;
    std::string body;

    try {
        connect = WinHttpConnect(session, std::wstring(host, parts.dwHostNameLength).c_str(), parts.nPort, 0);
        if (!connect) {
            throw std::runtime_error("WinHttpConnect failed.");
        }

        const DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        request = WinHttpOpenRequest(
            connect,
            L"GET",
            pathAndQuery.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags);
        if (!request) {
            throw std::runtime_error("WinHttpOpenRequest failed.");
        }

        std::wstring headers =
            L"Accept: " + acceptHeader + L"\r\n"
            L"Accept-Language: ko-KR,ko;q=0.9,en-US;q=0.7,en;q=0.6\r\n";

        if (!WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            throw std::runtime_error("WinHttpSendRequest failed.");
        }
        if (!WinHttpReceiveResponse(request, nullptr)) {
            throw std::runtime_error("WinHttpReceiveResponse failed.");
        }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX);
        if (status < 200 || status >= 300) {
            throw std::runtime_error("HTTP status " + std::to_string(status) + " for " + url);
        }

        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) {
                throw std::runtime_error("WinHttpQueryDataAvailable failed.");
            }
            if (available == 0) {
                break;
            }
            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read)) {
                throw std::runtime_error("WinHttpReadData failed.");
            }
            chunk.resize(read);
            body += chunk;
        }
    } catch (...) {
        if (request) {
            WinHttpCloseHandle(request);
        }
        if (connect) {
            WinHttpCloseHandle(connect);
        }
        WinHttpCloseHandle(session);
        throw;
    }

    if (request) {
        WinHttpCloseHandle(request);
    }
    if (connect) {
        WinHttpCloseHandle(connect);
    }
    WinHttpCloseHandle(session);
    return body;
}

std::string DigitsOnly(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return ch < '0' || ch > '9';
    }), value.end());
    return value;
}

std::string FirstRegexGroup(const std::string& text, const std::regex& re) {
    std::smatch match;
    if (std::regex_search(text, match, re) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

SiteResult FetchJobKorea() {
    SiteResult result;
    result.site = "JobKorea";
    result.sourceUrl = "https://www.jobkorea.co.kr/recruit/joblist?menucode=local&localorder=1";

    try {
        const std::string html = HttpGet(result.sourceUrl);
        std::string count = DigitsOnly(FirstRegexGroup(html, std::regex("id=\"hdnGICnt\"\\s+value=\"([^\"]+)\"", std::regex::icase)));
        if (count.empty()) {
            count = DigitsOnly(FirstRegexGroup(html, std::regex("hdnGICnt[^>]*value=\"([^\"]+)\"", std::regex::icase)));
        }
        if (count.empty()) {
            count = FirstRegexGroup(html, std::regex(R"("jobsLength"\s*:\s*([0-9]+))"));
        }
        if (count.empty()) {
            count = FirstRegexGroup(html, std::regex(R"(jobsLength\\?":\s*([0-9]+))"));
        }
        if (count.empty()) {
            count = FirstRegexGroup(html, std::regex(R"(resultCount\\?":\s*([0-9]+))"));
        }
        if (count.empty()) {
            throw std::runtime_error("Could not find total count in JobKorea joblist page.");
        }
        result.count = count;
        result.status = "ok";
        result.message = "parsed joblist hdnGICnt";
    } catch (const std::exception& ex) {
        result.status = "error";
        result.message = ex.what();
    }

    return result;
}

std::pair<std::string, std::string> NowKst() {
    std::time_t now = std::time(nullptr);
    now += 9 * 60 * 60;
    std::tm utc{};
    gmtime_s(&utc, &now);

    char timestamp[32]{};
    char date[16]{};
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &utc);
    std::strftime(date, sizeof(date), "%Y-%m-%d", &utc);
    return {timestamp, date};
}

std::string CsvEscape(const std::string& value) {
    bool needsQuote = false;
    for (char ch : value) {
        if (ch == ',' || ch == '"' || ch == '\r' || ch == '\n') {
            needsQuote = true;
            break;
        }
    }
    if (!needsQuote) {
        return value;
    }
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('"');
    return out;
}

std::vector<std::string> ParseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (quoted) {
            if (ch == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else if (ch == '"') {
                quoted = false;
            } else {
                current.push_back(ch);
            }
        } else {
            if (ch == '"') {
                quoted = true;
            } else if (ch == ',') {
                fields.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
    }
    fields.push_back(current);
    return fields;
}

std::vector<Record> ReadCsv(const std::filesystem::path& path) {
    std::vector<Record> records;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return records;
    }

    std::string line;
    bool first = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (first) {
            first = false;
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
            continue;
        }
        if (line.empty()) {
            continue;
        }
        const auto fields = ParseCsvLine(line);
        Record rec;
        if (fields.size() > 0) rec.timestampKst = fields[0];
        if (fields.size() > 1) rec.dateKst = fields[1];
        if (fields.size() > 2) rec.jobKoreaCount = fields[2];
        if (fields.size() > 3) rec.jobKoreaStatus = fields[3];
        if (fields.size() > 4) rec.jobKoreaMessage = fields[4];
        if (fields.size() > 5) rec.jobKoreaSource = fields[5];
        records.push_back(std::move(rec));
    }
    return records;
}

void WriteCsv(const std::filesystem::path& path, const std::vector<Record>& records) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot write CSV: " + path.string());
    }
    output << "\xEF\xBB\xBF";
    for (size_t i = 0; i < kHeaders.size(); ++i) {
        if (i) output << ',';
        output << kHeaders[i];
    }
    output << "\r\n";

    for (const auto& rec : records) {
        const std::vector<std::string> fields = {
            rec.timestampKst,
            rec.dateKst,
            rec.jobKoreaCount,
            rec.jobKoreaStatus,
            rec.jobKoreaMessage,
            rec.jobKoreaSource,
        };
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i) output << ',';
            output << CsvEscape(fields[i]);
        }
        output << "\r\n";
    }
}

std::string XmlEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (unsigned char ch : value) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:
            if (ch >= 0x20 || ch == '\n' || ch == '\r' || ch == '\t') {
                out.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return out;
}

std::string ColumnName(int oneBasedColumn) {
    std::string name;
    int col = oneBasedColumn;
    while (col > 0) {
        --col;
        name.push_back(static_cast<char>('A' + (col % 26)));
        col /= 26;
    }
    std::reverse(name.begin(), name.end());
    return name;
}

std::string CellRef(int oneBasedColumn, int oneBasedRow) {
    return ColumnName(oneBasedColumn) + std::to_string(oneBasedRow);
}

bool IsNumeric(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
}

bool TryParseDate(const std::string& value, std::tm& out) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(value.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }
    out = {};
    out.tm_year = year - 1900;
    out.tm_mon = month - 1;
    out.tm_mday = day;
    out.tm_hour = 12;
    return true;
}

std::string FormatDate(const std::tm& value) {
    char buffer[16]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &value);
    return buffer;
}

std::string WeekStartDate(const std::string& date) {
    std::tm parsed{};
    if (!TryParseDate(date, parsed)) {
        return "";
    }
    std::time_t time = std::mktime(&parsed);
    if (time == static_cast<std::time_t>(-1)) {
        return "";
    }
    std::tm normalized{};
    localtime_s(&normalized, &time);
    const int daysFromMonday = (normalized.tm_wday + 6) % 7;
    time -= static_cast<std::time_t>(daysFromMonday) * 24 * 60 * 60;
    std::tm weekStart{};
    localtime_s(&weekStart, &time);
    return FormatDate(weekStart);
}

std::vector<WeeklyCandle> BuildWeeklyCandles(const std::vector<Record>& records) {
    std::vector<WeeklyCandle> candles;
    for (const auto& record : records) {
        if (record.jobKoreaStatus != "ok" || !IsNumeric(record.jobKoreaCount)) {
            continue;
        }
        const std::string weekStart = WeekStartDate(record.dateKst);
        if (weekStart.empty()) {
            continue;
        }
        const int count = std::stoi(record.jobKoreaCount);
        if (candles.empty() || candles.back().weekStart != weekStart) {
            candles.push_back({weekStart, count, count, count, count});
        } else {
            auto& candle = candles.back();
            candle.high = std::max(candle.high, count);
            candle.low = std::min(candle.low, count);
            candle.close = count;
        }
    }
    return candles;
}

std::string TextCell(int col, int row, const std::string& value, int style = 0) {
    std::ostringstream xml;
    xml << "<c r=\"" << CellRef(col, row) << "\" t=\"inlineStr\"";
    if (style) xml << " s=\"" << style << "\"";
    xml << "><is><t>" << XmlEscape(value) << "</t></is></c>";
    return xml.str();
}

std::string NumberCell(int col, int row, const std::string& value, int style = 0) {
    std::ostringstream xml;
    xml << "<c r=\"" << CellRef(col, row) << "\"";
    if (style) xml << " s=\"" << style << "\"";
    xml << "><v>" << value << "</v></c>";
    return xml.str();
}

std::string DataWorksheetXml(const std::vector<Record>& records) {
    std::ostringstream xml;
    const int lastRow = static_cast<int>(records.size()) + 1;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    xml << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
    xml << "<cols>"
        << "<col min=\"1\" max=\"2\" width=\"22\" customWidth=\"1\"/>"
        << "<col min=\"3\" max=\"3\" width=\"16\" customWidth=\"1\"/>"
        << "<col min=\"4\" max=\"5\" width=\"24\" customWidth=\"1\"/>"
        << "<col min=\"6\" max=\"6\" width=\"72\" customWidth=\"1\"/>"
        << "</cols><sheetData>";
    xml << "<row r=\"1\">";
    for (int i = 0; i < static_cast<int>(kHeaders.size()); ++i) {
        xml << TextCell(i + 1, 1, kHeaders[static_cast<size_t>(i)], 1);
    }
    xml << "</row>";

    int row = 2;
    for (const auto& rec : records) {
        const std::vector<std::string> fields = {
            rec.timestampKst,
            rec.dateKst,
            rec.jobKoreaCount,
            rec.jobKoreaStatus,
            rec.jobKoreaMessage,
            rec.jobKoreaSource,
        };
        xml << "<row r=\"" << row << "\">";
        for (int col = 1; col <= static_cast<int>(fields.size()); ++col) {
            const std::string& value = fields[static_cast<size_t>(col - 1)];
            if (col == 3 && IsNumeric(value)) {
                xml << NumberCell(col, row, value);
            } else {
                xml << TextCell(col, row, value);
            }
        }
        xml << "</row>";
        ++row;
    }
    xml << "</sheetData>";
    xml << "<autoFilter ref=\"A1:F" << std::max(1, lastRow) << "\"/>";
    xml << "<pageMargins left=\"0.7\" right=\"0.7\" top=\"0.75\" bottom=\"0.75\" header=\"0.3\" footer=\"0.3\"/>";
    xml << "</worksheet>";
    return xml.str();
}

std::string WeeklyWorksheetXml(const std::vector<WeeklyCandle>& candles) {
    const std::vector<std::string> headers = {"WeekStart", "Open", "High", "Low", "Close"};
    const int lastRow = static_cast<int>(candles.size()) + 1;
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    xml << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
    xml << "<cols><col min=\"1\" max=\"1\" width=\"18\" customWidth=\"1\"/>"
        << "<col min=\"2\" max=\"5\" width=\"14\" customWidth=\"1\"/></cols><sheetData>";
    xml << "<row r=\"1\">";
    for (int i = 0; i < static_cast<int>(headers.size()); ++i) {
        xml << TextCell(i + 1, 1, headers[static_cast<size_t>(i)], 1);
    }
    xml << "</row>";

    int row = 2;
    for (const auto& candle : candles) {
        xml << "<row r=\"" << row << "\">";
        xml << TextCell(1, row, candle.weekStart);
        xml << NumberCell(2, row, std::to_string(candle.open));
        xml << NumberCell(3, row, std::to_string(candle.high));
        xml << NumberCell(4, row, std::to_string(candle.low));
        xml << NumberCell(5, row, std::to_string(candle.close));
        xml << "</row>";
        ++row;
    }
    xml << "</sheetData>";
    xml << "<autoFilter ref=\"A1:E" << std::max(1, lastRow) << "\"/>";
    xml << "<pageMargins left=\"0.7\" right=\"0.7\" top=\"0.75\" bottom=\"0.75\" header=\"0.3\" footer=\"0.3\"/>";
    xml << "</worksheet>";
    return xml.str();
}

std::string ChartWorksheetXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
           "<sheetData/>"
           "<drawing r:id=\"rId1\"/>"
           "<pageMargins left=\"0.7\" right=\"0.7\" top=\"0.75\" bottom=\"0.75\" header=\"0.3\" footer=\"0.3\"/>"
           "</worksheet>";
}

std::string DrawingXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
           "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
           "<xdr:twoCellAnchor>"
           "<xdr:from><xdr:col>0</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>0</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>"
           "<xdr:to><xdr:col>12</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>24</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:to>"
           "<xdr:graphicFrame macro=\"\">"
           "<xdr:nvGraphicFramePr><xdr:cNvPr id=\"2\" name=\"Job Post Count Chart\"/>"
           "<xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr>"
           "<xdr:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/></xdr:xfrm>"
           "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
           "<c:chart xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" r:id=\"rId1\"/>"
           "</a:graphicData></a:graphic>"
           "</xdr:graphicFrame><xdr:clientData/></xdr:twoCellAnchor></xdr:wsDr>";
}

std::string LineSeriesXml(int idx, const std::string& name, const std::string& valueColumn, int lastRow) {
    std::ostringstream xml;
    xml << "<c:ser>"
        << "<c:idx val=\"" << idx << "\"/><c:order val=\"" << idx << "\"/>"
        << "<c:tx><c:v>" << XmlEscape(name) << "</c:v></c:tx>"
        << "<c:cat><c:strRef><c:f>Data!$A$2:$A$" << lastRow << "</c:f></c:strRef></c:cat>"
        << "<c:val><c:numRef><c:f>Data!$" << valueColumn << "$2:$" << valueColumn << "$" << lastRow << "</c:f></c:numRef></c:val>"
        << "</c:ser>";
    return xml.str();
}

std::string ChartXml(const std::vector<Record>& records) {
    const int lastRow = std::max(2, static_cast<int>(records.size()) + 1);
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    xml << "<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
           "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
    xml << "<c:chart><c:title><c:tx><c:rich><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>Job Posting Counts</a:t></a:r></a:p></c:rich></c:tx></c:title>";
    xml << "<c:plotArea><c:layout/><c:lineChart><c:grouping val=\"standard\"/>"
        << LineSeriesXml(0, "JobKorea", "C", lastRow)
        << "<c:axId val=\"123456\"/><c:axId val=\"123457\"/></c:lineChart>";
    xml << "<c:catAx><c:axId val=\"123456\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
        << "<c:delete val=\"0\"/><c:axPos val=\"b\"/><c:tickLblPos val=\"nextTo\"/><c:crossAx val=\"123457\"/><c:crosses val=\"autoZero\"/></c:catAx>";
    xml << "<c:valAx><c:axId val=\"123457\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
        << "<c:delete val=\"0\"/><c:axPos val=\"l\"/><c:majorGridlines/><c:numFmt formatCode=\"#,##0\" sourceLinked=\"0\"/>"
        << "<c:tickLblPos val=\"nextTo\"/><c:crossAx val=\"123456\"/><c:crosses val=\"autoZero\"/></c:valAx>";
    xml << "</c:plotArea><c:legend><c:legendPos val=\"b\"/><c:layout/></c:legend><c:plotVisOnly val=\"1\"/></c:chart></c:chartSpace>";
    return xml.str();
}

std::string StockSeriesXml(int idx, const std::string& name, const std::string& valueColumn, int lastRow) {
    std::ostringstream xml;
    xml << "<c:ser>"
        << "<c:idx val=\"" << idx << "\"/><c:order val=\"" << idx << "\"/>"
        << "<c:tx><c:v>" << XmlEscape(name) << "</c:v></c:tx>"
        << "<c:cat><c:strRef><c:f>WeeklyOHLC!$A$2:$A$" << lastRow << "</c:f></c:strRef></c:cat>"
        << "<c:val><c:numRef><c:f>WeeklyOHLC!$" << valueColumn << "$2:$" << valueColumn << "$" << lastRow << "</c:f></c:numRef></c:val>"
        << "</c:ser>";
    return xml.str();
}

std::string WeeklyCandleChartXml(const std::vector<WeeklyCandle>& candles) {
    const int lastRow = std::max(2, static_cast<int>(candles.size()) + 1);
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    xml << "<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
           "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
    xml << "<c:chart><c:title><c:tx><c:rich><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>Weekly Job Posting Candles</a:t></a:r></a:p></c:rich></c:tx></c:title>";
    xml << "<c:plotArea><c:layout/><c:stockChart>"
        << StockSeriesXml(0, "Open", "B", lastRow)
        << StockSeriesXml(1, "High", "C", lastRow)
        << StockSeriesXml(2, "Low", "D", lastRow)
        << StockSeriesXml(3, "Close", "E", lastRow)
        << "<c:hiLowLines/>"
        << "<c:upDownBars><c:gapWidth val=\"150\"/><c:upBars/><c:downBars/></c:upDownBars>"
        << "<c:axId val=\"223456\"/><c:axId val=\"223457\"/></c:stockChart>";
    xml << "<c:catAx><c:axId val=\"223456\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
        << "<c:delete val=\"0\"/><c:axPos val=\"b\"/><c:tickLblPos val=\"nextTo\"/><c:crossAx val=\"223457\"/><c:crosses val=\"autoZero\"/></c:catAx>";
    xml << "<c:valAx><c:axId val=\"223457\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
        << "<c:delete val=\"0\"/><c:axPos val=\"l\"/><c:majorGridlines/><c:numFmt formatCode=\"#,##0\" sourceLinked=\"0\"/>"
        << "<c:tickLblPos val=\"nextTo\"/><c:crossAx val=\"223456\"/><c:crosses val=\"autoZero\"/></c:valAx>";
    xml << "</c:plotArea><c:legend><c:legendPos val=\"b\"/><c:layout/></c:legend><c:plotVisOnly val=\"1\"/></c:chart></c:chartSpace>";
    return xml.str();
}

uint32_t Crc32(const std::string& data) {
    static uint32_t table[256] = {};
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
            }
            table[i] = c;
        }
        initialized = true;
    }
    uint32_t crc = 0xFFFFFFFFU;
    for (unsigned char ch : data) {
        crc = table[(crc ^ ch) & 0xFFU] ^ (crc >> 8U);
    }
    return crc ^ 0xFFFFFFFFU;
}

void WriteU16(std::ostream& os, uint16_t value) {
    os.put(static_cast<char>(value & 0xFFU));
    os.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void WriteU32(std::ostream& os, uint32_t value) {
    os.put(static_cast<char>(value & 0xFFU));
    os.put(static_cast<char>((value >> 8U) & 0xFFU));
    os.put(static_cast<char>((value >> 16U) & 0xFFU));
    os.put(static_cast<char>((value >> 24U) & 0xFFU));
}

struct ZipEntry {
    std::string name;
    std::string data;
    uint32_t crc = 0;
    uint32_t offset = 0;
};

void WriteStoredZip(const std::filesystem::path& path, std::vector<ZipEntry> entries) {
    std::ofstream os(path, std::ios::binary);
    if (!os) {
        throw std::runtime_error("Cannot write XLSX: " + path.string());
    }

    for (auto& entry : entries) {
        entry.crc = Crc32(entry.data);
        entry.offset = static_cast<uint32_t>(os.tellp());
        WriteU32(os, 0x04034B50U);
        WriteU16(os, 20);
        WriteU16(os, 0x0800);
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU32(os, entry.crc);
        WriteU32(os, static_cast<uint32_t>(entry.data.size()));
        WriteU32(os, static_cast<uint32_t>(entry.data.size()));
        WriteU16(os, static_cast<uint16_t>(entry.name.size()));
        WriteU16(os, 0);
        os.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
        os.write(entry.data.data(), static_cast<std::streamsize>(entry.data.size()));
    }

    const uint32_t centralOffset = static_cast<uint32_t>(os.tellp());
    for (const auto& entry : entries) {
        WriteU32(os, 0x02014B50U);
        WriteU16(os, 20);
        WriteU16(os, 20);
        WriteU16(os, 0x0800);
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU32(os, entry.crc);
        WriteU32(os, static_cast<uint32_t>(entry.data.size()));
        WriteU32(os, static_cast<uint32_t>(entry.data.size()));
        WriteU16(os, static_cast<uint16_t>(entry.name.size()));
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU16(os, 0);
        WriteU32(os, 0);
        WriteU32(os, entry.offset);
        os.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
    }
    const uint32_t centralSize = static_cast<uint32_t>(os.tellp()) - centralOffset;

    WriteU32(os, 0x06054B50U);
    WriteU16(os, 0);
    WriteU16(os, 0);
    WriteU16(os, static_cast<uint16_t>(entries.size()));
    WriteU16(os, static_cast<uint16_t>(entries.size()));
    WriteU32(os, centralSize);
    WriteU32(os, centralOffset);
    WriteU16(os, 0);
}

void WriteXlsx(const std::filesystem::path& path, const std::vector<Record>& records) {
    const std::vector<WeeklyCandle> weeklyCandles = BuildWeeklyCandles(records);
    std::vector<ZipEntry> entries;
    entries.push_back({
        "[Content_Types].xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet3.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet4.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "<Override PartName=\"/xl/drawings/drawing1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>"
        "<Override PartName=\"/xl/drawings/drawing2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>"
        "<Override PartName=\"/xl/charts/chart1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawingml.chart+xml\"/>"
        "<Override PartName=\"/xl/charts/chart2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawingml.chart+xml\"/>"
        "</Types>"
    });
    entries.push_back({
        "_rels/.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>"
    });
    entries.push_back({
        "xl/workbook.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"Data\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"Chart\" sheetId=\"2\" r:id=\"rId2\"/>"
        "<sheet name=\"WeeklyOHLC\" sheetId=\"3\" r:id=\"rId3\"/>"
        "<sheet name=\"WeeklyCandles\" sheetId=\"4\" r:id=\"rId4\"/></sheets>"
        "</workbook>"
    });
    entries.push_back({
        "xl/_rels/workbook.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet3.xml\"/>"
        "<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet4.xml\"/>"
        "<Relationship Id=\"rId5\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>"
    });
    entries.push_back({
        "xl/styles.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font><font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"2\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/></cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>"
    });
    entries.push_back({"xl/worksheets/sheet1.xml", DataWorksheetXml(records)});
    entries.push_back({"xl/worksheets/sheet2.xml", ChartWorksheetXml()});
    entries.push_back({"xl/worksheets/sheet3.xml", WeeklyWorksheetXml(weeklyCandles)});
    entries.push_back({"xl/worksheets/sheet4.xml", ChartWorksheetXml()});
    entries.push_back({
        "xl/worksheets/_rels/sheet2.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing1.xml\"/>"
        "</Relationships>"
    });
    entries.push_back({
        "xl/worksheets/_rels/sheet4.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing2.xml\"/>"
        "</Relationships>"
    });
    entries.push_back({"xl/drawings/drawing1.xml", DrawingXml()});
    entries.push_back({"xl/drawings/drawing2.xml", DrawingXml()});
    entries.push_back({
        "xl/drawings/_rels/drawing1.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/>"
        "</Relationships>"
    });
    entries.push_back({
        "xl/drawings/_rels/drawing2.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart2.xml\"/>"
        "</Relationships>"
    });
    entries.push_back({"xl/charts/chart1.xml", ChartXml(records)});
    entries.push_back({"xl/charts/chart2.xml", WeeklyCandleChartXml(weeklyCandles)});
    WriteStoredZip(path, std::move(entries));
}

Options ParseOptions(int argc, wchar_t** argv) {
    Options options;
    options.outputDir = std::filesystem::current_path() / "data";

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--output-dir" && i + 1 < argc) {
            options.outputDir = argv[++i];
        } else if (arg == L"--skip-jobkorea") {
            options.skipJobKorea = true;
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            std::cout << "Usage: JobPostCounter.exe [--output-dir data] [--skip-jobkorea]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown or incomplete option: " + ToUtf8(arg));
        }
    }
    return options;
}

Record BuildRecord(const Options& options) {
    const auto [timestamp, date] = NowKst();
    Record rec;
    rec.timestampKst = timestamp;
    rec.dateKst = date;

    SiteResult jobKorea;
    if (options.skipJobKorea) {
        jobKorea = {"JobKorea", "", "skipped", "skipped by option", "https://www.jobkorea.co.kr/"};
    } else {
        jobKorea = FetchJobKorea();
    }

    rec.jobKoreaCount = jobKorea.count;
    rec.jobKoreaStatus = jobKorea.status;
    rec.jobKoreaMessage = jobKorea.message;
    rec.jobKoreaSource = jobKorea.sourceUrl;
    return rec;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        const Options options = ParseOptions(argc, argv);
        std::filesystem::create_directories(options.outputDir);

        const std::filesystem::path csvPath = options.outputDir / "job_post_counts.csv";
        const std::filesystem::path xlsxPath = options.outputDir / "job_post_counts.xlsx";

        auto records = ReadCsv(csvPath);
        Record rec = BuildRecord(options);
        records.push_back(rec);

        WriteCsv(csvPath, records);
        WriteXlsx(xlsxPath, records);

        std::cout << "Saved: " << csvPath.string() << "\n";
        std::cout << "Saved: " << xlsxPath.string() << "\n";
        std::cout << "JobKorea: " << rec.jobKoreaStatus << " " << rec.jobKoreaCount << " " << rec.jobKoreaMessage << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
