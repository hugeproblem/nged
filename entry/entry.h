#pragma once
#include <string>
#include <string_view>

namespace nged {

// you will need to implement these functions
class App {
public:
  virtual ~App() {}
  virtual char const* title() { return "Demo App"; }
  virtual bool        agreeToQuit() { return true; }
  virtual void        init();
  virtual void        update() = 0;
  virtual void        quit() {};
};

void startApp(App* app);

std::wstring utf8towstring(std::string_view str);

}

