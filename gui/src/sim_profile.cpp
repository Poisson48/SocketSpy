#include "sim_profile.h"
#include <QFile>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace socketspy::gui {

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
