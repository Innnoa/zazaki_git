#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>

#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace zazaki_git {

using ConfigValue = std::variant<std::string, bool, long long, double>;

struct ConfigSection {
    std::unordered_map<std::string, ConfigValue> values;
};

class Config {
public:
    Config() { set_defaults(); }

    bool load(const std::string& filepath);
    bool load_string(const std::string& content);

    std::optional<ConfigValue> get(const std::string& section,
                                   const std::string& key) const;

    std::string get_string(const std::string& section,
                           const std::string& key,
                           const std::string& default_val = "") const;

    bool get_bool(const std::string& section,
                  const std::string& key,
                  bool default_val = false) const;

    const std::unordered_map<std::string, ConfigSection>& sections() const {
        return sections_;
    }

    ftxui::Event to_event(const std::string& name) const;
    ftxui::Color to_color(const std::string& name) const;

    ftxui::Event keybinding(const std::string& section,
                            const std::string& action) const;

    ftxui::Color color(const std::string& section,
                       const std::string& key) const;

private:
    void set_defaults();
    void parse_line(const std::string& line, std::string& current_section);
    static std::string trim(const std::string& s);

    std::unordered_map<std::string, ConfigSection> sections_;
    std::unordered_map<std::string, ftxui::Event> event_map_;
    std::unordered_map<std::string, ftxui::Color> color_map_;
};

inline bool Config::load(const std::string& filepath) {
    FILE* f = std::fopen(filepath.c_str(), "r");
    if (!f) return false;

    std::string content;
    char buf[4096];
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), f)) {
        content.append(buf, n);
    }
    std::fclose(f);
    return load_string(content);
}

inline bool Config::load_string(const std::string& content) {
    std::string current_section;
    std::string line;

    for (char c : content) {
        if (c == '\n') {
            parse_line(line, current_section);
            line.clear();
        } else if (c == '\r') {
            continue;
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        parse_line(line, current_section);
    }
    return true;
}

inline std::optional<ConfigValue> Config::get(const std::string& section,
                                              const std::string& key) const {
    auto sit = sections_.find(section);
    if (sit == sections_.end()) return std::nullopt;
    auto vit = sit->second.values.find(key);
    if (vit == sit->second.values.end()) return std::nullopt;
    return vit->second;
}

inline std::string Config::get_string(const std::string& section,
                                      const std::string& key,
                                      const std::string& default_val) const {
    auto val = get(section, key);
    if (!val) return default_val;
    if (auto* s = std::get_if<std::string>(&*val)) return *s;
    return default_val;
}

inline bool Config::get_bool(const std::string& section,
                             const std::string& key,
                             bool default_val) const {
    auto val = get(section, key);
    if (!val) return default_val;
    if (auto* b = std::get_if<bool>(&*val)) return *b;
    return default_val;
}

inline ftxui::Event Config::to_event(const std::string& name) const {
    auto it = event_map_.find(name);
    if (it != event_map_.end()) return it->second;

    if (name.size() > 5 && name.compare(0, 5, "Ctrl+") == 0) {
        char key = static_cast<char>(std::tolower(name[5]));
        switch (key) {
            case 'a': return ftxui::Event::CtrlA;
            case 'b': return ftxui::Event::CtrlB;
            case 'c': return ftxui::Event::CtrlC;
            case 'd': return ftxui::Event::CtrlD;
            case 'e': return ftxui::Event::CtrlE;
            case 'f': return ftxui::Event::CtrlF;
            case 'g': return ftxui::Event::CtrlG;
            case 'h': return ftxui::Event::CtrlH;
            case 'i': return ftxui::Event::CtrlI;
            case 'j': return ftxui::Event::CtrlJ;
            case 'k': return ftxui::Event::CtrlK;
            case 'l': return ftxui::Event::CtrlL;
            case 'm': return ftxui::Event::CtrlM;
            case 'n': return ftxui::Event::CtrlN;
            case 'o': return ftxui::Event::CtrlO;
            case 'p': return ftxui::Event::CtrlP;
            case 'q': return ftxui::Event::CtrlQ;
            case 'r': return ftxui::Event::CtrlR;
            case 's': return ftxui::Event::CtrlS;
            case 't': return ftxui::Event::CtrlT;
            case 'u': return ftxui::Event::CtrlU;
            case 'v': return ftxui::Event::CtrlV;
            case 'w': return ftxui::Event::CtrlW;
            case 'x': return ftxui::Event::CtrlX;
            case 'y': return ftxui::Event::CtrlY;
            case 'z': return ftxui::Event::CtrlZ;
            default: break;
        }
    }

    if (name.size() == 1) {
        return ftxui::Event::Character(name[0]);
    }

    return ftxui::Event::Custom;
}

inline ftxui::Color Config::to_color(const std::string& name) const {
    auto it = color_map_.find(name);
    if (it != color_map_.end()) return it->second;
    return ftxui::Color::Default;
}

inline ftxui::Event Config::keybinding(const std::string& section,
                                       const std::string& action) const {
    auto name = get_string(section, action);
    if (name.empty()) return ftxui::Event::Custom;
    return to_event(name);
}

inline ftxui::Color Config::color(const std::string& section,
                                  const std::string& key) const {
    auto name = get_string(section, key);
    if (name.empty()) return ftxui::Color::Default;
    return to_color(name);
}

inline void Config::parse_line(const std::string& line,
                               std::string& current_section) {
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return;

    if (trimmed[0] == '[' && trimmed.back() == ']') {
        current_section = trim(std::string(trimmed, 1, trimmed.size() - 2));
        if (sections_.find(current_section) == sections_.end()) {
            sections_[current_section] = ConfigSection{};
        }
        return;
    }

    auto eq = trimmed.find('=');
    if (eq == std::string::npos) return;

    auto key = trim(std::string(trimmed, 0, eq));
    auto value = trim(std::string(trimmed, eq + 1));

    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = std::string(value, 1, value.size() - 2);
    }

    if (value == "true") {
        sections_[current_section].values[key] = true;
    } else if (value == "false") {
        sections_[current_section].values[key] = false;
    } else {
        bool is_number = true;
        bool has_dot = false;
        for (size_t i = 0; i < value.size(); ++i) {
            char c = value[i];
            if (c == '-' && i == 0) continue;
            if (c == '.') { has_dot = true; continue; }
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                is_number = false;
                break;
            }
        }
        if (is_number && !value.empty() && !(value.size() == 1 && value[0] == '-')) {
            if (has_dot) {
                sections_[current_section].values[key] = std::stod(value);
            } else {
                sections_[current_section].values[key] = std::stoll(value);
            }
        } else {
            sections_[current_section].values[key] = value;
        }
    }
}

inline std::string Config::trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return std::string(s, start, end - start);
}

inline void Config::set_defaults() {
    event_map_ = {
        {"Space",    ftxui::Event::Character(' ')},
        {"Enter",    ftxui::Event::Return},
        {"Escape",   ftxui::Event::Escape},
        {"Tab",      ftxui::Event::Tab},
        {"ShiftTab", ftxui::Event::TabReverse},
        {"Backspace",ftxui::Event::Backspace},
        {"Delete",   ftxui::Event::Delete},
        {"Up",       ftxui::Event::ArrowUp},
        {"Down",     ftxui::Event::ArrowDown},
        {"Left",     ftxui::Event::ArrowLeft},
        {"Right",    ftxui::Event::ArrowRight},
        {"Home",     ftxui::Event::Home},
        {"End",      ftxui::Event::End},
        {"PageUp",   ftxui::Event::PageUp},
        {"PageDown", ftxui::Event::PageDown},
        {"Insert",   ftxui::Event::Insert},
        {"F1",       ftxui::Event::F1},
        {"F2",       ftxui::Event::F2},
        {"F3",       ftxui::Event::F3},
        {"F4",       ftxui::Event::F4},
        {"F5",       ftxui::Event::F5},
        {"F6",       ftxui::Event::F6},
        {"F7",       ftxui::Event::F7},
        {"F8",       ftxui::Event::F8},
        {"F9",       ftxui::Event::F9},
        {"F10",      ftxui::Event::F10},
        {"F11",      ftxui::Event::F11},
        {"F12",      ftxui::Event::F12},
    };

    color_map_ = {
        {"Default",        ftxui::Color::Default},
        {"Black",          ftxui::Color::Black},
        {"Red",            ftxui::Color::Red},
        {"Green",          ftxui::Color::Green},
        {"Yellow",         ftxui::Color::Yellow},
        {"Blue",           ftxui::Color::Blue},
        {"Magenta",        ftxui::Color::Magenta},
        {"Cyan",           ftxui::Color::Cyan},
        {"GrayLight",      ftxui::Color::GrayLight},
        {"GrayDark",       ftxui::Color::GrayDark},
        {"RedLight",       ftxui::Color::RedLight},
        {"GreenLight",     ftxui::Color::GreenLight},
        {"YellowLight",    ftxui::Color::YellowLight},
        {"BlueLight",      ftxui::Color::BlueLight},
        {"MagentaLight",   ftxui::Color::MagentaLight},
        {"CyanLight",      ftxui::Color::CyanLight},
        {"White",          ftxui::Color::White},
        {"Grey0",          ftxui::Color::Grey0},
        {"Grey100",        ftxui::Color::Grey100},
        {"Grey11",         ftxui::Color::Grey11},
        {"Grey15",         ftxui::Color::Grey15},
        {"Grey19",         ftxui::Color::Grey19},
        {"Grey23",         ftxui::Color::Grey23},
        {"Grey27",         ftxui::Color::Grey27},
        {"Grey3",          ftxui::Color::Grey3},
        {"Grey30",         ftxui::Color::Grey30},
        {"Grey35",         ftxui::Color::Grey35},
        {"Grey37",         ftxui::Color::Grey37},
        {"Grey39",         ftxui::Color::Grey39},
        {"Grey42",         ftxui::Color::Grey42},
        {"Grey46",         ftxui::Color::Grey46},
        {"Grey50",         ftxui::Color::Grey50},
        {"Grey53",         ftxui::Color::Grey53},
        {"Grey54",         ftxui::Color::Grey54},
        {"Grey58",         ftxui::Color::Grey58},
        {"Grey62",         ftxui::Color::Grey62},
        {"Grey63",         ftxui::Color::Grey63},
        {"Grey66",         ftxui::Color::Grey66},
        {"Grey69",         ftxui::Color::Grey69},
        {"Grey7",          ftxui::Color::Grey7},
        {"Grey70",         ftxui::Color::Grey70},
        {"Grey74",         ftxui::Color::Grey74},
        {"Grey78",         ftxui::Color::Grey78},
        {"Grey82",         ftxui::Color::Grey82},
        {"Grey84",         ftxui::Color::Grey84},
        {"Grey85",         ftxui::Color::Grey85},
        {"Grey89",         ftxui::Color::Grey89},
        {"Grey93",         ftxui::Color::Grey93},
    };
}

}  // namespace zazaki_git
