#define _CRT_SECURE_NO_WARNINGS 1
#include "s7.h"
#include <string>
#include <chrono>
#include <regex>
#include <filesystem>

template <class T>
class OptionalArg
{
public:
  T value;
  operator T() const { return value; }
  OptionalArg(T value) :value(std::move(value)) {}
};
template <class T>
static inline OptionalArg<T> optional(T val)
{
  return OptionalArg(val);
}
template <class T>
struct IsOptionalArg { static constexpr bool value = false; };
template <class T>
struct IsOptionalArg<OptionalArg<T>> { static constexpr bool value = true; };

// s7_string    -> std::string 
// s7_int       -> int64_t
// s7_double    -> double
// s7_character -> char
// s7_t / s7_f  -> bool
template <class T>
static inline s7_pointer extractOneArg(s7_scheme* sc, char const* caller, int ntharg, s7_pointer arg, T& ret)
{
  if constexpr (std::is_same<s7_int, T>::value) {
    if (s7_is_integer(arg))
      ret = s7_integer(arg);
    else if (s7_is_number(arg))
      ret = static_cast<s7_int>(s7_real(arg));
    else
      return s7_wrong_type_arg_error(sc, caller, ntharg, arg, "integer");
  } else if constexpr (std::is_same<s7_double, T>::value) {
    if (s7_is_number(arg))
      ret = s7_real(arg);
    else
      return s7_wrong_type_arg_error(sc, caller, ntharg, arg, "number");
  } else if constexpr (std::is_same<char, T>::value || std::is_same<uint8_t, T>::value) {
    if (s7_is_character(arg))
      ret = s7_character(arg);
    else
      return s7_wrong_type_arg_error(sc, caller, ntharg, arg, "character");
  } else if constexpr (std::is_same<bool, T>::value) {
    if (s7_is_boolean(arg))
      ret = s7_boolean(sc, arg);
    else
      return s7_wrong_type_arg_error(sc, caller, ntharg, arg, "boolean");
  } else if constexpr (std::is_same<std::string, T>::value) {
    if (s7_is_string(arg))
      ret = s7_string(arg);
    else if (s7_is_symbol(arg))
      ret = s7_symbol_name(arg);
    else
      return s7_wrong_type_arg_error(sc, caller, ntharg, arg, "string or symbol");
  } else {
    static_assert(std::is_same<bool, T>::value, "unsupported C++ type");
    return s7_f(sc);
  }
  return s7_t(sc);
}

template <class T>
static inline s7_pointer extractOneArgMaybeOptional(s7_scheme* sc, char const* caller, int ntharg, s7_pointer arg, T& ret)
{
  if constexpr (IsOptionalArg<T>::value) {
    if (arg == s7_nil(sc))
      return s7_t(sc);
    return extractOneArg(sc, caller, ntharg, s7_car(arg), ret.value);
  } else {
    return extractOneArg(sc, caller, ntharg, s7_car(arg), ret);
  }
}

static inline s7_pointer extractArgs_(s7_scheme* sc, char const* caller, s7_pointer args, int ntharg, s7_pointer currentArg)
{
  if (currentArg!=s7_nil(sc)) {
    return s7_wrong_number_of_args_error(sc, caller, args);
  }
  return s7_t(sc);
}

template <class T, class... Rest>
static inline s7_pointer extractArgs_(s7_scheme* sc, char const* caller, s7_pointer args, int ntharg, s7_pointer currentArg, T& value, Rest&... rest)
{
  auto t = s7_t(sc);
  s7_pointer restargs = nullptr;
  if (currentArg != s7_nil(sc)) {
    restargs = s7_cdr(currentArg);
  } else {
    restargs = currentArg;
    if constexpr (!IsOptionalArg<T>::value) {
      return s7_wrong_number_of_args_error(sc, caller, args);
    }
  }
  auto result = extractOneArgMaybeOptional(sc, caller, ntharg, currentArg, value);
  if (result != t)
    return result;
  return extractArgs_(sc, caller, args, ntharg+1, restargs, rest...);
}


/// Usage Example:
/// 
///   std::string s;
///   double      d;
///   auto        optb = optional(false);
///   if (auto ac = extractAllArgs(sc, "my-function-name", args, s, d, optb); ac!=s7_t(sc))
///     return ac;
///   /* from here on you can use s, d and optb variables */
///
/// Supported primitive types are:
///
///   s7_string    -> std::string 
///   s7_int       -> int64_t
///   s7_double    -> double
///   s7_character -> char
///   s7_t / s7_f  -> bool
template <class... T>
static s7_pointer extractAllArgs(s7_scheme* sc, char const* caller, s7_pointer args, T& ...values)
{
  return extractArgs_(sc, caller, args, 0, args, values...);
}

static char const* const extraCode = R"S7EXTRA((begin
(define (vector-head v end)
  (subvector v 0 end))

(define (vector-tail v start)
  (subvector v start (length v)))

(define (string-head s end)
  (substring s 0 end))

(define (string-tail s start)
  (substring s start (length s)))

(define (string-starts-with s start)
  (string=? start
    (substring s 0 (length start))))

(define (string-ends-with s end)
  (string=? end
    (substring s (- (length s) (length end)) (length s))))

#|
; this implement may cause GC crash
(define (filter f lst)
  (let loop ((result '()) (rest lst))
    (if (= (length rest) 0)
        result
        (append result (if (f (car rest)) (list (car rest)) '()) (loop result (cdr rest))))))
|#

(define (empty? lst) (eqv? lst '()))

(define
 (filter judge lst)
 (reverse
  (let loop ((result '()) (rest lst))
   (if (empty? rest)
    result
    (loop
     (if (judge (car rest))
      (cons (car rest) result)
      result)
     (cdr rest))))))

(define (foldl proc initial lst)
  (let loop ((acc initial) (rst lst))
    (if (empty? rst)
      acc
      (loop (proc acc (car rst)) (cdr rst)))))

(define reduce foldl)

(define (curry proc . args) (lambda rest-args (apply proc (append args rest-args))))

(define (curryl proc . args) (lambda rest-args (apply proc (append rest-args args))))

(define
 (combine . funcs)
 (if (empty? funcs)
  (error "nothing to combine"))
 (letrec
  ((combined
    (lambda
     (funcs args)
     (if (empty? (cdr funcs))
      (apply (car funcs) args)
      ((car funcs)
       (combined (cdr funcs) args))))))
  (lambda args (combined funcs args))))

(define (sprintf . args)
 (let ((buf (open-output-string)))
  (apply format (cons buf args))
  (get-output-string buf)))

(define fprintf format)
(define printf
 (lambda args
  (let ((cp (current-output-port)))
   (apply format (cons cp args)))))

(define-macro
 (bind . expr)
 (let
  ((max_arg
    (let
     loop
     ((remaining-expr expr) (curmax 0))
     (if
      (empty? remaining-expr)
      curmax
      (if
       (list? (car remaining-expr))
       (max
        (loop (car remaining-expr) curmax)
        (loop (cdr remaining-expr) curmax))
       (if
        (or
         (not (symbol? (car remaining-expr)))
         (empty?
          (regex-match
           "_(\\d+)"
           (symbol->string (car remaining-expr)))))
        (loop (cdr remaining-expr) curmax)
        (max
         (loop (cdr remaining-expr) curmax)
         (string->number
          ((regex-match
            "_(\\d+)"
            (symbol->string (car remaining-expr)))
           1)))))))))
  `(lambda
    ,(let
      loop
      ((arglist '()) (i max_arg))
      (if
       (< i 1)
       arglist
       (loop
        (cons
         (string->symbol
          (string-append "_" (number->string i)))
         arglist)
        (- i 1))))
    ,@expr)))
))S7EXTRA";

static s7_pointer cpp_dir_to_list(s7_scheme* sc, s7_pointer args)
{
  #define H_directory_to_list "(directory->list directory [recursive]) returns the contents of the directory as a list of strings (filenames)."
  #define Q_directory_to_list s7_make_signature(sc, 3, s7_make_symbol(sc,"list?"), s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"boolean?"))   /* can return nil */

  s7_pointer result = nullptr;

  std::filesystem::path path = "";

  std::string strpath;
  auto recursive = optional(false);
  auto ac = extractAllArgs(sc, "directory->list", args, strpath, recursive);
  if (ac != s7_t(sc))
    return ac;
  path = std::filesystem::path(strpath);

  result = s7_make_list(sc, 0, nullptr);
  if (recursive) {
    for (auto diritr : std::filesystem::recursive_directory_iterator(path)) {
      std::string u8path = diritr.path().u8string();
      result = s7_cons(sc, s7_make_string_with_length(sc, u8path.c_str(), u8path.length()), result);
    }
  } else {
    for (auto diritr : std::filesystem::directory_iterator(path)) {
      std::string u8path = diritr.path().u8string();
      result = s7_cons(sc, s7_make_string_with_length(sc, u8path.c_str(), u8path.length()), result);
    }
  }
  return s7_reverse(sc, result);
}

static s7_pointer cpp_make_dirs(s7_scheme* sc, s7_pointer args)
{
  #define H_make_dirs "(make-dirs path) makes directory at path if it does not exist, returns #t if succeed"
  #define Q_make_dirs s7_make_signature(sc, 2, s7_make_symbol(sc,"boolean?"), s7_make_symbol(sc,"string?"))

  std::string strpath;
  if (auto ac = extractAllArgs(sc, "make-dirs", args, strpath); ac != s7_t(sc))
    return ac;
  try {
    return s7_make_boolean(sc, std::filesystem::create_directories(std::filesystem::path(strpath)));
  } catch (std::filesystem::filesystem_error const& e) {
    return s7_error(sc, s7_make_symbol(sc, "filesystem-error"), s7_make_string(sc, e.what()));
  } catch (std::exception const& e) {
    return s7_error(sc, s7_make_symbol(sc, "c++-error"), s7_make_string(sc, e.what()));
  }
  return s7_f(sc);
}

static s7_pointer cpp_file_exists(s7_scheme* sc, s7_pointer args)
{
  #define H_file_exists "(file-exists? filename) returns #t if the file exists"
  #define Q_file_exists s7_make_signature(sc, 2, s7_make_symbol(sc,"boolean?"), s7_make_symbol(sc,"string?"))

  s7_pointer name = s7_car(args);
  if (!s7_is_string(name))
    return(s7_wrong_type_arg_error(sc, "file-exists?", 1, name, "string"));
  return(s7_make_boolean(sc, std::filesystem::exists(s7_string(name))));
}

static s7_pointer cpp_file_mtime(s7_scheme* sc, s7_pointer args)
{
  #define H_file_mtime "(file-mtime file): return the write date of file"
  #define Q_file_mtime s7_make_signature(sc, 2, s7_make_symbol(sc,"integer?"), s7_make_symbol(sc,"string?"))

  s7_pointer name = s7_car(args);
  if (!s7_is_string(name))
    return(s7_wrong_type_arg_error(sc, "file-mtime", 1, name, "string"));
  auto mtime = std::filesystem::last_write_time(s7_string(name));
  // TODO: this is only a approximation
  //       in c++20 we will have std::chrono::file_clock::to_sys
  static const auto fstimenow = decltype(mtime)::clock::now();
  static const auto systimenow = std::chrono::system_clock::now();
  time_t systime = std::chrono::system_clock::to_time_t((mtime-fstimenow)+systimenow);
  return(s7_make_integer(sc, systime));
}

static s7_pointer cpp_format_time(s7_scheme* sc, s7_pointer args)
{
  #define H_format_time "(format-time time format timezone):\n see https://en.cppreference.com/w/c/chrono/strftime for format document,\n default format is \"%%Y-%%m-%%d %%H:%%M:%%S\";\n timezone can be 'utc or 'local"
  #define Q_format_time s7_make_signature(sc, 3, s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"integer?"), s7_make_symbol(sc,"string?"), s7_make_symbol(sc, "symbol?"))

  s7_int ti;
  auto   optfmt = optional<std::string>("%Y-%m-%d %H:%M:%S");
  auto   opttz = optional<std::string>("local");
  if (auto ac = extractAllArgs(sc, "format-time", args, ti, optfmt, opttz); ac != s7_t(sc))
    return ac;
  time_t tt = static_cast<time_t>(ti);
  struct tm* tm = nullptr;
  if (opttz.value == "utc")
    tm = gmtime(&tt);
  else if (opttz.value == "local")
    tm = localtime(&tt);
  else
    return s7_wrong_type_arg_error(sc, "format-time", 2, args, "should be one of 'local or 'utc");

  char buf[128] = { 0 };
  auto sz = strftime(buf, sizeof(buf), optfmt.value.c_str(), tm);
  return s7_make_string(sc, buf);
}

static s7_pointer cpp_is_directory(s7_scheme* sc, s7_pointer args)
{
#define H_is_directory "(directory? str) returns #t if str is the name of a directory"
#define Q_is_directory s7_make_signature(sc, 2, s7_make_symbol(sc,"boolean?"), s7_make_symbol(sc,"string?"))

  s7_pointer name = s7_car(args);
  if (!s7_is_string(name))
    return(s7_wrong_type_arg_error(sc, "directory?", 1, name, "string"));
  return(s7_make_boolean(sc, std::filesystem::is_directory(s7_string(name))));
}

#define Q_path_decomposition s7_make_signature(sc, 2, s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"string?"))
#define Q_path_query         s7_make_signature(sc, 2, s7_make_symbol(sc,"boolean?"), s7_make_symbol(sc,"string?"))
#define REGISTER_PATH_DECOMP(op, name)\
    s7_define_typed_function(sc, "path-" name, cpp_path_##op, 1, 0, false, H_path_##op, Q_path_decomposition)
#define REGISTER_PATH_QUERY(op, name)\
    s7_define_typed_function(sc, "path-" name, cpp_path_##op, 1, 0, false, H_path_##op, Q_path_query)

#define DEFINE_PATH_DECOMPOSITION(op, name)\
static constexpr char const * const H_path_##op = "path-" name " returns path." #op "()";\
static s7_pointer cpp_path_##op(s7_scheme* sc, s7_pointer args)\
{\
  s7_pointer str = s7_car(args);\
  if (!s7_is_string(str))\
    return(s7_wrong_type_arg_error(sc, name, 1, str, "string"));\
  auto path = std::filesystem::path(s7_string(str));\
  return(s7_make_string(sc, path.op().u8string().c_str()));\
}
#define DEFINE_PATH_QUERY(op, name)\
static constexpr char const * const H_path_##op = "path-" name " returns path." #op "()";\
static s7_pointer cpp_path_##op(s7_scheme* sc, s7_pointer args)\
{\
  s7_pointer str = s7_car(args);\
  if (!s7_is_string(str))\
    return(s7_wrong_type_arg_error(sc, name, 1, str, "string"));\
  auto path = std::filesystem::path(s7_string(str));\
  return(s7_make_boolean(sc, path.op()));\
}

DEFINE_PATH_DECOMPOSITION(root_name, "root-name")
DEFINE_PATH_DECOMPOSITION(root_directory, "root-directory")
DEFINE_PATH_DECOMPOSITION(root_path, "root-path")
DEFINE_PATH_DECOMPOSITION(relative_path, "relative-path")
DEFINE_PATH_DECOMPOSITION(parent_path, "parent-path")
DEFINE_PATH_DECOMPOSITION(filename, "filename")
DEFINE_PATH_DECOMPOSITION(extension, "extension")
DEFINE_PATH_DECOMPOSITION(stem, "stem")

DEFINE_PATH_QUERY(is_absolute, "absolute?")
DEFINE_PATH_QUERY(is_relative, "relative?")
DEFINE_PATH_QUERY(has_root_path, "has-root-path?")
DEFINE_PATH_QUERY(has_root_name, "has-root-name?")
DEFINE_PATH_QUERY(has_root_directory, "has-root-directory?")
DEFINE_PATH_QUERY(has_relative_path, "has-relative-path?")
DEFINE_PATH_QUERY(has_parent_path, "has-parent-path?")
DEFINE_PATH_QUERY(has_filename, "has-filename?")
DEFINE_PATH_QUERY(has_extension, "has-extension?")
DEFINE_PATH_QUERY(has_stem, "has-stem?")

static s7_pointer cpp_regex_match(s7_scheme* sc, s7_pointer args)
{
#define H_regex_match "(regex-match regex str): match str with regex, returns matched pairs"
#define Q_regex_match s7_make_signature(sc, 3, s7_make_symbol(sc,"list?"), s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"string?"))
  std::string re, str;
  auto ac = extractAllArgs(sc, "regex-match", args, re, str);
  if (ac != s7_t(sc)) {
    return ac;
  }
  std::regex regex(re);
  std::smatch matches;
  if (std::regex_match(str, matches, regex)) {
    s7_pointer lst = s7_list(sc, 0);
    for (int n = int(matches.size())-1; n >= 0; --n) {
      lst = s7_cons(sc, s7_make_string(sc, matches[n].str().c_str()), lst);
    }
    return lst;
  } else {
    return(s7_list(sc, 0));
  }
}

static s7_pointer cpp_regex_replace(s7_scheme* sc, s7_pointer args)
{
#define H_regex_replace "(regex-replace regex fmt str): replace regex with fmt in str"
#define Q_regex_replace s7_make_signature(sc, 4, s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"string?"), s7_make_symbol(sc,"string?"))
   
  std::string re, fmt, str;
  auto ac = extractAllArgs(sc, "regex-replace", args, re, fmt, str);
  if (ac != s7_t(sc)) {
    return ac;
  }
  std::regex regex(re);
  try {
    std::string result = std::regex_replace(str, regex, fmt);
    return s7_make_string_with_length(sc, result.c_str(), result.size());
  } catch (std::regex_error const& e) {
    return s7_error(sc, s7_make_symbol(sc, "bad-result"), s7_make_string(sc, e.what()));
  }
  return s7_list(sc, 0);
}

static s7_pointer cpp_string_split(s7_scheme* sc, s7_pointer args)
{
#define H_string_split "(string-split str sep): split str with sep"
#define Q_string_split s7_make_signature(sc, 3, s7_make_symbol(sc, "list?"), s7_make_symbol(sc, "string?"), s7_make_symbol(sc, "string?"))
  std::string str, sep;
  if (auto ac = extractAllArgs(sc, "string-split", args, str, sep); ac != s7_t(sc)) {
    return ac;
  }
  auto car = s7_nil(sc);
  for (size_t i = 0, ni = 0; ni != std::string::npos; i = ni + 1) {
    ni = str.find(sep, i);
    auto part = str.substr(i, ni == std::string::npos ? ni : ni - i);
    car = s7_cons(sc, s7_make_string_with_length(sc, part.c_str(), part.length()), car);
  }
  return s7_reverse(sc, car);
}

extern "C" void addS7Extenstions(s7_scheme* sc)
{
  if (sc) {
    s7_define_typed_function(sc, "directory->list", cpp_dir_to_list, 1, 1, false, H_directory_to_list, Q_directory_to_list);
    s7_define_typed_function(sc, "file-exists?",    cpp_file_exists, 1, 0, false, H_file_exists, Q_file_exists);
    s7_define_typed_function(sc, "file-mtime",      cpp_file_mtime,  1, 0, false, H_file_mtime,  Q_file_mtime);
    s7_define_typed_function(sc, "format-time",     cpp_format_time, 1, 2, false, H_format_time,  Q_format_time);
    s7_define_typed_function(sc, "directory?",      cpp_is_directory,1, 0, false, H_is_directory,Q_is_directory);
    s7_define_typed_function(sc, "make-dirs",       cpp_make_dirs,   1, 0, false, H_make_dirs, Q_make_dirs);
    REGISTER_PATH_DECOMP(root_name, "root-name");
    REGISTER_PATH_DECOMP(root_directory, "root-directory");
    REGISTER_PATH_DECOMP(root_path, "root-path");
    REGISTER_PATH_DECOMP(relative_path, "relative-path");
    REGISTER_PATH_DECOMP(parent_path, "parent-path");
    REGISTER_PATH_DECOMP(filename, "filename");
    REGISTER_PATH_DECOMP(extension, "extension");
    REGISTER_PATH_DECOMP(stem, "stem");
    REGISTER_PATH_QUERY(is_absolute, "absolute?");
    REGISTER_PATH_QUERY(is_relative, "relative?");
    REGISTER_PATH_QUERY(has_root_path, "has-root-path?");
    REGISTER_PATH_QUERY(has_root_name, "has-root-name?");
    REGISTER_PATH_QUERY(has_root_directory, "has-root-directory?");
    REGISTER_PATH_QUERY(has_relative_path, "has-relative-path?");
    REGISTER_PATH_QUERY(has_parent_path, "has-parent-path?");
    REGISTER_PATH_QUERY(has_filename, "has-filename?");
    REGISTER_PATH_QUERY(has_extension, "has-extension?");
    REGISTER_PATH_QUERY(has_stem, "has-stem?");
    s7_define_typed_function(sc, "regex-match",   cpp_regex_match,   2, 0, false, H_regex_match, Q_regex_match);
    s7_define_typed_function(sc, "regex-replace", cpp_regex_replace, 3, 0, false, H_regex_replace, Q_regex_replace);
    s7_define_typed_function(sc, "string-split", cpp_string_split, 2, 0, false, H_string_split, Q_string_split);
    s7_eval_c_string(sc, extraCode);
  }
}
