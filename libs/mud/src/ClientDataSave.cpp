#include <volcano/mud/ClientDataSave.hpp>

#include <nlohmann/json.hpp>

namespace volcano::mud {

void to_json(nlohmann::json& j, const ClientData& capabilities) {
    j[+"client_protocol"] = capabilities.client_protocol;
    j[+"client_name"] = capabilities.client_name;
    j[+"client_version"] = capabilities.client_version;
    j[+"encoding"] = capabilities.encoding;
    j[+"tls"] = capabilities.tls;
    j[+"color"] = capabilities.color;
    j[+"width"] = capabilities.width;
    j[+"height"] = capabilities.height;
    j[+"mssp"] = capabilities.mssp;
    j[+"mccp2"] = capabilities.mccp2;
    j[+"mccp2_enabled"] = capabilities.mccp2_enabled;
    j[+"mccp3"] = capabilities.mccp3;
    j[+"mccp3_enabled"] = capabilities.mccp3_enabled;
    j[+"gmcp"] = capabilities.gmcp;
    j[+"mtts"] = capabilities.mtts;
    j[+"naws"] = capabilities.naws;
    j[+"charset"] = capabilities.charset;
    j[+"mnes"] = capabilities.mnes;
    j[+"linemode"] = capabilities.linemode;
    j[+"sga"] = capabilities.sga;
    j[+"force_endline"] = capabilities.force_endline;
    j[+"screen_reader"] = capabilities.screen_reader;
    j[+"mouse_tracking"] = capabilities.mouse_tracking;
    j[+"vt100"] = capabilities.vt100;
    j[+"osc_color_palette"] = capabilities.osc_color_palette;
    j[+"proxy"] = capabilities.proxy;
    j[+"tls_support"] = capabilities.tls_support;
}

void from_json(const nlohmann::json& j, ClientData& capabilities) {
    if(j.contains(+"client_protocol")) j.at(+"client_protocol").get_to(capabilities.client_protocol);
    if(j.contains(+"client_name")) j.at(+"client_name").get_to(capabilities.client_name);
    if(j.contains(+"client_version")) j.at(+"client_version").get_to(capabilities.client_version);
    if(j.contains(+"encoding")) j.at(+"encoding").get_to(capabilities.encoding);
    if(j.contains(+"tls")) j.at(+"tls").get_to(capabilities.tls);
    if(j.contains(+"color")) j.at(+"color").get_to(capabilities.color);
    if(j.contains(+"width")) j.at(+"width").get_to(capabilities.width);
    if(j.contains(+"height")) j.at(+"height").get_to(capabilities.height);
    if(j.contains(+"mssp")) j.at(+"mssp").get_to(capabilities.mssp);
    if(j.contains(+"mccp2")) j.at(+"mccp2").get_to(capabilities.mccp2);
    if(j.contains(+"mccp2_enabled")) j.at(+"mccp2_enabled").get_to(capabilities.mccp2_enabled);
    if(j.contains(+"mccp3")) j.at(+"mccp3").get_to(capabilities.mccp3);
    if(j.contains(+"mccp3_enabled")) j.at(+"mccp3_enabled").get_to(capabilities.mccp3_enabled);
    if(j.contains(+"gmcp")) j.at(+"gmcp").get_to(capabilities.gmcp);
    if(j.contains(+"mtts")) j.at(+"mtts").get_to(capabilities.mtts);
    if(j.contains(+"naws")) j.at(+"naws").get_to(capabilities.naws);
    if(j.contains(+"charset")) j.at(+"charset").get_to(capabilities.charset);
    if(j.contains(+"mnes")) j.at(+"mnes").get_to(capabilities.mnes);
    if(j.contains(+"linemode")) j.at(+"linemode").get_to(capabilities.linemode);
    if(j.contains(+"sga")) j.at(+"sga").get_to(capabilities.sga);
    if(j.contains(+"force_endline")) j.at(+"force_endline").get_to(capabilities.force_endline);
    if(j.contains(+"screen_reader")) j.at(+"screen_reader").get_to(capabilities.screen_reader);
    if(j.contains(+"mouse_tracking")) j.at(+"mouse_tracking").get_to(capabilities.mouse_tracking);
    if(j.contains(+"vt100")) j.at(+"vt100").get_to(capabilities.vt100);
    if(j.contains(+"osc_color_palette")) j.at(+"osc_color_palette").get_to(capabilities.osc_color_palette);
    if(j.contains(+"proxy")) j.at(+"proxy").get_to(capabilities.proxy);
    if(j.contains(+"tls_support")) j.at(+"tls_support").get_to(capabilities.tls_support);
}

} // namespace dbat::mud
