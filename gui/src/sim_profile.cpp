#include "sim_profile.h"
#include <QFile>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace socketspy::gui {

static WaveformType waveformFromInt(int v) {
    switch (v) {
    case 1: return WaveformType::Sine;
    case 2: return WaveformType::Ramp;
    case 3: return WaveformType::Square;
    case 4: return WaveformType::Random;
    default: return WaveformType::None;
    }
}

static SimSignal parse_signal(const json& j) {
    SimSignal s;
    s.name          = QString::fromStdString(j.at("name").get<std::string>());
    s.start_bit     = j.at("start_bit").get<int>();
    s.length        = j.at("length").get<int>();
    s.factor        = j.value("factor",  1.0);
    s.offset        = j.value("offset",  0.0);
    s.min           = j.value("min",     0.0);
    s.max           = j.value("max",     255.0);
    s.current_value = j.value("default", 0.0);
    s.waveform      = waveformFromInt(j.value("waveform",   0));
    s.num_points    = j.value("num_points", 100);
    s.step_ms       = j.value("step_ms",    50);
    if (j.contains("scenario")) {
        for (const auto& pt : j.at("scenario")) {
            SimScenarioPoint p;
            p.t_ms  = static_cast<int64_t>(pt.at("t_ms").get<double>());
            p.value = pt.at("value").get<double>();
            s.scenario.push_back(p);
        }
    }
    return s;
}

static SimMessage parse_message(const json& j) {
    SimMessage m;
    std::string id_str = j.at("id").get<std::string>();
    m.id        = static_cast<uint32_t>(std::stoul(id_str, nullptr, 0));
    m.period_ms = j.at("period_ms").get<int>();
    m.dlc       = static_cast<uint8_t>(j.at("dlc").get<int>());
    for (const auto& sig : j.at("signals"))
        m.sigs.push_back(parse_signal(sig));
    return m;
}

static SimNode parse_node(const json& j) {
    SimNode n;
    n.name = QString::fromStdString(j.at("name").get<std::string>());
    for (const auto& msg : j.at("messages"))
        n.messages.push_back(parse_message(msg));
    return n;
}

SimProfile load_sim_profile(const QString& json_path) {
    QFile f(json_path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray raw = f.readAll();
    try {
        json j = json::parse(raw.constData(), raw.constData() + raw.size());
        SimProfile p;
        p.name        = QString::fromStdString(j.at("name").get<std::string>());
        p.description = QString::fromStdString(j.value("description", std::string{}));
        for (const auto& node : j.at("nodes"))
            p.nodes.push_back(parse_node(node));
        return p;
    } catch (...) {
        return {};
    }
}

} // namespace socketspy::gui
