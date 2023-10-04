#pragma once
namespace parmscript {

class Parm;
class ParmSetInspector;

bool inspectFilePath(Parm& parm);
bool inspectDirPath(Parm& parm);
bool inspectCode(Parm& parm);

void addExtensions();

} // namespace parmscript
