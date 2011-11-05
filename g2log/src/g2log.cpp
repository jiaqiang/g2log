/* *************************************************
 * Filename:g2log.cpp  Framework for Logging and Design By Contract
 * Created: 2011 by Kjell Hedström
 *
 * PUBLIC DOMAIN and Not copywrited since it was built on public-domain software and influenced
 * from the following sources
 * 1. kjellkod.cc ;)
 * 2. Dr.Dobbs, Petru Marginean:  http://drdobbs.com/article/printableArticle.jhtml?articleId=201804215&dept_url=/cpp/
 * 3. Dr.Dobbs, Michael Schulze: http://drdobbs.com/article/printableArticle.jhtml?articleId=225700666&dept_url=/cpp/
 * 4. Google 'glog': http://google-glog.googlecode.com/svn/trunk/doc/glog.html
 * 5. Various Q&A at StackOverflow
 * ********************************************* */

#include "g2log.h"

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept> // exceptions
#include <cstdio>    // vsnprintf
#include <cassert>
#include <mutex>
#include "logworker.h"

namespace g2
{
namespace constants
{
const int kMaxMessageSize = 2048;
const std::string kTruncatedWarningText = "[...truncated...]";
}
namespace internal
{
static LogWorker* g_logger_instance = nullptr; // instantiated and OWNED somewhere else (main)
static std::mutex g_logging_init_mutex;
bool isLoggingInitialized(){return g_logger_instance != nullptr; }

/** thanks to: http://www.cplusplus.com/reference/string/string/find_last_of/
* Splits string at the last '/' or '\\' separator
* example: "/mnt/something/else.cpp" --> "else.cpp"
*          "c:\\windows\\hello.h" --> hello.h
*          "this.is.not-a-path.h" -->"this.is.not-a-path.h" */
std::string splitFileName(const std::string& str)
{
  size_t found;
  found = str.find_last_of("(/\\");
  return str.substr(found+1);
}

} // end namespace g2::internal


void initializeLogging(LogWorker *bgworker)
{
  std::lock_guard<std::mutex> lock(internal::g_logging_init_mutex);
  CHECK(!internal::isLoggingInitialized());
  CHECK(bgworker != nullptr);
  internal::g_logger_instance = bgworker;
}

LogWorker* shutDownLogging()
{
  std::lock_guard<std::mutex> lock(internal::g_logging_init_mutex);
  CHECK(internal::isLoggingInitialized());
  LogWorker *backup = internal::g_logger_instance;
  internal::g_logger_instance = nullptr;
  return backup;
}




namespace internal
{
LogContractMessage::LogContractMessage(const std::string &file, const int line,
                                       const std::string& function, const std::string &boolean_expression)
  : LogMessage(file, line, function, "FATAL")
  , expression_(boolean_expression)
{}

LogContractMessage::~LogContractMessage()
{
  std::ostringstream oss;
  if(0 == expression_.compare(k_fatal_log_expression))
  {
    oss << "[  *******\tRUNTIME EXCEPTION caused by LOG(FATAL):\t";
  }
  else
  {
    oss << "\nRUNTIME EXCEPTION caused by broken Contract: [" << expression_ << "]\t";
  }
  log_entry_ = oss.str();
}

LogMessage::LogMessage(const std::string &file, const int line, const std::string& function, const std::string &level)
  : file_(file)
  , line_(line)
  , function_(function)
  , level_(level)

{}


LogMessage::~LogMessage()
{
  std::ostringstream oss;
  const bool fatal = (0 == level_.compare("FATAL"));
  oss << level_ << " [" << internal::splitFileName(file_);
  if(fatal)
    oss <<  " F: " << function_ ;
  oss << " L: " << line_ << "]\t";

  const std::string str(stream_.str());
  if(!str.empty())
  {
    oss << '"' << str << '"';
  }
  log_entry_ += oss.str();

  if(!internal::isLoggingInitialized() )
  {
    std::cerr << "Did you forget to call g2::InitializeLogging(LogWorker*) in your main.cpp?" << std::endl;
    std::cerr << log_entry_ << std::endl << std::flush;
    throw std::runtime_error("Logger not initialized with g2::InitializeLogging(LogWorker*) for msg:\n" + log_entry_);
  }

  internal::g_logger_instance->save(log_entry_); // message saved
  if(fatal)
  {
    std::cerr  << log_entry_ << "\t*******  ]" << std::endl << std::flush;
    throw std::runtime_error(log_entry_);
  }


}


void LogMessage::messageSave(const char *printf_like_message, ...)
{
  char finished_message[constants::kMaxMessageSize];
  va_list arglist;
  va_start(arglist, printf_like_message);
  const int nbrcharacters = vsnprintf(finished_message, sizeof(finished_message), printf_like_message, arglist);
  va_end(arglist);
  if (nbrcharacters <= 0)
  {
    stream_ << "\n\tERROR LOG MSG NOTIFICATION: Failure to parse successfully the message";
    stream_ << '"' << printf_like_message << '"' << std::endl;
  }
  else if (nbrcharacters > constants::kMaxMessageSize)
  {
    stream_  << finished_message << constants::kTruncatedWarningText;
  }
  else
  {
    stream_ << finished_message;
  }
}

} // end of namespace g2::internal
} // end of namespace g2