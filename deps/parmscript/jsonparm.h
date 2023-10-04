#pragma once

#include <nlohmann/json_fwd.hpp>

namespace parmscript {

class Parm;
class ParmSet;

void from_json(nlohmann::json const& j, Parm& p);
void from_json(nlohmann::json const& j, ParmSet& p);
void to_json(nlohmann::json& j, Parm const& p);
void to_json(nlohmann::json& j, ParmSet const& p);

}

