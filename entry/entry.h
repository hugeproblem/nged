// you will need to implement these functions
namespace nged {

class App {
public:
  virtual ~App() {}
  virtual char    const* title() { return "Demo App"; }
  virtual wchar_t const* wtitle();
  virtual bool           agreeToQuit() { return true; }
  virtual void           init();
  virtual void           update() = 0;
  virtual void           quit() {};
};

void startApp(App* app);

}

