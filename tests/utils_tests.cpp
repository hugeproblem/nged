#include <doctest/doctest.h>
#include <utils.h>

namespace doctest {
String toString(std::string_view s) { return String(s.data(), s.size()); }
}

TEST_CASE("String Split") {
  std::string str = "hello\nworld\n";
  auto sep = utils::strsplit(str, "\n");
  CHECK(sep.size()==2);
  CHECK(sep[0] == "hello");
  CHECK(sep[1] == "world");

  sep = utils::strsplit("foo;;barr;;xx", ";;");
  CHECK(sep.size()==3);
  CHECK(sep[0] == "foo");
  CHECK(sep[1] == "barr");
  CHECK(sep[2] == "xx");

  sep = utils::strsplit("foo", "\n");
  CHECK(sep.size()==1);
  CHECK(sep[0] == "foo");
}

TEST_CASE("String Strip") {
  using utils::strstrip;
  CHECK(strstrip("  hello world") == "hello world");
  CHECK(strstrip("foo bar   ") == "foo bar");
  CHECK(strstrip("  hi there $_#_+", " $_#+") == "hi there");
  CHECK(strstrip("    ") == "");
}
