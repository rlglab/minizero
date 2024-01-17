#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32

namespace minizero::utils::tqdm {

#include <windows.h>

inline int getTerminalWidth()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
}

} // namespace minizero::utils::tqdm

#else

#include <sys/ioctl.h>
#include <unistd.h>

namespace minizero::utils::tqdm {

inline int getTerminalWidth()
{
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    return size.ws_col;
}

} // namespace minizero::utils::tqdm

#endif

namespace minizero::utils::tqdm {

struct Format {
    std::string key;
    std::string data;
};

/**
 * @brief Parse single format string.
 *
 * @param format Format string (e.g. "key;data", data can be any string).
 *
 * @return Format
 */
inline Format parseSingleFormatString(const std::string& format)
{
    Format fmt;
    bool is_key = true;
    bool is_escape = false;
    for (size_t i = 0; i < format.length(); ++i) {
        if (is_escape) {
            if (is_key) {
                fmt.key += format.at(i);
            } else {
                fmt.data += format.at(i);
            }
            is_escape = false;
        } else {
            switch (format.at(i)) {
                case ';':
                    if (is_key) {
                        is_key = false;
                    } else {
                        fmt.data += format.at(i);
                    }
                    break;
                case '\\':
                    is_escape = true;
                    break;
                default:
                    if (is_key) {
                        fmt.key += format.at(i);
                    } else {
                        fmt.data += format.at(i);
                    }
                    break;
            }
        }
    }
    return fmt;
}

struct Match {
    size_t start, end;
    Format fmt;
};

/**
 * @brief Parse format string.
 *
 * @param format_string Format string (e.g. "...{key1;data1}...{key2;data2}...").
 *
 * @return std::vector<Match> (e.g. {{start1, end1, {key1, data1}}, {start2, end2, {key2, data2}}}).
 */
inline std::vector<Match> parseFormatString(const std::string& format_string)
{
    std::vector<Match> matches;
    bool is_format = false;
    bool is_escape = false;
    std::string format;
    size_t start = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < format_string.length(); ++i) {
        if (is_escape) {
            is_escape = false;
            if (is_format) { format += format_string.at(i); }
        } else {
            switch (format_string.at(i)) {
                case '{':
                    if (is_format) { throw std::runtime_error("invalid format string at " + std::to_string(i) + ": " + format_string); }
                    is_format = true;
                    start = i;
                    break;
                case '}':
                    if (!is_format) { throw std::runtime_error("invalid format string at " + std::to_string(i) + ": " + format_string); }
                    is_format = false;
                    matches.push_back({start, i, parseSingleFormatString(format)});
                    format.clear();
                    break;
                case '\\':
                    is_escape = true;
                    break;
                default:
                    if (is_format) { format += format_string.at(i); }
                    break;
            }
        }
    }
    return matches;
}

/**
 * @brief Replace format string with given data.
 *
 * @param format_string Format string (e.g. "...{key1;data1}...{key2;data2}...").
 * @param matches Matches the matches returned by parseFormatString (e.g. {{start1, end1, {key1, data1}}, {start2, end2, {key2, data2}}}).
 * @param data The data to replace (e.g. {key1: value1, key2: value2}).
 *
 * @return std::string (e.g. "...value1...value2...").
 */
inline std::string formatString(const std::string& format_string, const std::vector<Match>& matches, const std::unordered_map<std::string, std::string>& data)
{
    std::string result = format_string;
    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        auto data_it = data.find(it->fmt.key);
        if (data_it == data.end()) { continue; }
        result.replace(it->start, it->end - it->start + 1, data_it->second);
    }
    return result;
}

/**
 * @brief Replace format string with given data.
 *
 * @param format_string Format string (e.g. "...{key1;data1}...{key2;data2}...").
 * @param data The data to replace (e.g. {key1: value1, key2: value2}).
 *
 * @return std::string (e.g. "...value1...value2...").
 */
inline std::string formatString(const std::string& format_string, const std::unordered_map<std::string, std::string>& data)
{
    return formatString(format_string, parseFormatString(format_string), data);
}

/**
 * @brief A class representing a customizable progress bar.
 *
 * The ProgressBar class creates and manages a visual progress bar with customizable width and display patterns.
 * It allows users to set the width, display patterns, and percentage of completion.
 */
class ProgressBar {
public:
    /**
     * @brief Constructor for the ProgressBar class.
     *
     * @param width The width of the progress bar.
     * @param patterns Vector of strings representing the display patterns (defaults to {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"}).
     */
    ProgressBar(int width, std::vector<std::string> patterns = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"})
        : width_(width), patterns_(std::move(patterns)) {}

    void setWidth(int width) { width_ = width; }
    int getWidth() const { return width_; }
    void setPatterns(std::vector<std::string> patterns) { patterns_ = std::move(patterns); }
    const std::vector<std::string>& getPatterns() const { return patterns_; }
    void setPercentage(float percentage) { percentage_ = percentage; }
    float getPercentage() const { return percentage_; }

    std::string toString() const
    {
        if (width_ <= 0) return {};
        std::string bar;
        int pattern_num = patterns_.size() - 1;
        int num = (percentage_ - 1e-5) * width_ * pattern_num;
        for (int i = 0; i < num / pattern_num; ++i) {
            bar += patterns_.back();
        }
        bar += patterns_[num % pattern_num + 1];
        for (int i = 0; i < width_ - num / pattern_num - 1; ++i) {
            bar += patterns_.front();
        }
        return bar;
    }

    friend std::ostream& operator<<(std::ostream& os, const ProgressBar& bar) { return os << bar.toString(); }

private:
    int width_;
    std::vector<std::string> patterns_;
    float percentage_;
};

/**
 * @brief IteratorHook class to create an iterator wrapper with hooks.
 *
 * This class provides a mechanism to create an iterator with hooks, allowing additional actions or functions to be triggered at specific points during iteration.
 *
 * @tparam IteratorType The type of the underlying iterator to be wrapped.
 */
template <class IteratorType>
class IteratorHook {
public:
    /**
     * @brief Inner Iterator class to manage the wrapped iterator with hooks.
     */
    class Iterator {
    public:
        typedef typename IteratorType::difference_type difference_type;
        typedef typename IteratorType::value_type value_type;
        typedef typename IteratorType::reference reference;
        typedef typename IteratorType::pointer pointer;
        typedef std::forward_iterator_tag iterator_category;

        Iterator() : it_(IteratorType()), hook_(nullptr) {}
        Iterator(const Iterator& it) : it_(it.it_), hook_(nullptr) {}
        Iterator(const IteratorType& it, std::function<void(IteratorType)>* hook) : it_(it), hook_(hook) {}

        Iterator& operator=(const Iterator& it)
        {
            it_ = it.it_;
            hook_ = it.hook_;
            return *this;
        }
        bool operator==(const Iterator& it) const { return it_ == it.it_; }
        bool operator!=(const Iterator& it) const { return it_ != it.it_; }
        Iterator& operator++()
        {
            if (hook_) (*hook_)(++it_);
            return *this;
        }
        reference operator*() const { return *it_; }
        pointer operator->() const { return it_.operator->(); }

        IteratorType it_;
        std::function<void(IteratorType)>* hook_;
    };

    /**
     * @brief Constructor to create an IteratorHook object with specified begin and end iterators and a hook function.
     *
     * @tparam HookType The type of the hook function.
     * @param begin_it The starting iterator of the range.
     * @param end_it The ending iterator of the range.
     * @param hook The hook function to be associated with the iterators.
     */
    template <class HookType>
    IteratorHook(const IteratorType& begin_it, const IteratorType& end_it, HookType&& hook)
        : begin_it_(begin_it), end_it_(end_it), hook_(std::forward<HookType>(hook)) {}

    Iterator begin()
    {
        hook_(begin_it_);
        return Iterator(begin_it_, &hook_);
    }
    Iterator end() { return Iterator(end_it_, &hook_); }

    IteratorType begin_it_;
    IteratorType end_it_;
    std::function<void(IteratorType)> hook_;
};

/**
 * @brief Function to create an IteratorHook instance for a range of iterators with a specified hook function.
 *
 * This function generates an IteratorHook instance for a given range defined by the beginning and ending iterators.
 * It associates the provided hook function with the iterators, enabling additional actions to be performed during iteration through the range.
 *
 * @tparam IteratorType The type of the underlying iterators for the range.
 * @tparam HookType The type of the hook function.
 * @param begin_it The starting iterator of the range.
 * @param end_it The ending iterator of the range.
 * @param hook The hook function to be associated with the iterators.
 *
 * @return IteratorHook<IteratorType> An IteratorHook instance managing the iteration over the specified range with the provided hook function.
 */
template <class IteratorType, class HookType>
inline IteratorHook<IteratorType> makeIteratorRangeHook(const IteratorType& begin_it, const IteratorType& end_it, HookType&& hook)
{
    return {begin_it, end_it, std::forward<HookType>(hook)};
}

/**
 * @brief Tqdm class to display a progress bar in the console with estimated and remaining time.
 *
 * This class represents a progress bar using ASCII characters to display the progress of an operation.
 * It tracks the progress, displays it as a percentage-based progress bar, and also calculates and displays
 * estimated and remaining time for the operation.
 */
class Tqdm {
    using TimeType = std::chrono::system_clock::time_point;

public:
    /**
     * @brief Constructor to initialize the Tqdm object with size, title, and width of the progress bar.
     *
     * @param size The total size of the operation or iterations.
     * @param format The format string for the progress bar (defaults to "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]").
     * @param mininterval The minimum interval between updates in milliseconds (defaults to 100).
     * @param width The width of the progress bar (defaults to 0, which will be set to the width of the terminal).
     *
     * @note The format string can include user-defined text and keywords enclosed in curly braces, such as {key}.
     * @note The following special keywords will be replaced automatically:
     * @note  - {title}: The title for the progress bar (defaults to "").
     * @note  - {percentage}: The percentage of the progress.
     * @note  - {pbar}: The progress bar.
     * @note  - {step}: The current step value.
     * @note  - {size}: The total size of the operation or iterations.
     * @note  - {es}: The estimated time for the operation to complete. Users can customize the format using additional specifiers, e.g., {es;d:h:m:s} will display in the format of "dd:hh:mm:ss".
     * @note  - {re}: The remaining time for the operation to complete. Similar to {es}, users can define custom formats and choose specific time units to display.
     * @note  - {ps}: The passed time for the operation. Like {es} and {re}, users have the flexibility to tailor the format and displayed time units as needed.
     * @note Users can define custom keywords by enclosing them in curly braces, e.g., {custom_key}.
     * @note Custom keywords can be replaced by calling set("custom_key", "value").
     */
    Tqdm(size_t size, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", int mininterval = 100, int width = 0)
        : size_(size), format_(format), mininterval_(mininterval), width_(width), progress_bar_(0)
    {
        data_["title"] = "";
        reset();
    }

    /**
     * @brief Resets the progress bar state to initial values and sets the start time.
     */
    void reset()
    {
        step_ = 0;
        first_print_ = true;
        start_time_ = std::chrono::system_clock::now();
    }

    /**
     * @brief Increments the progress step, updates the current time, and returns the current step value.
     *
     * @return size_t The current step value after incrementing.
     */
    size_t step()
    {
        if (step_ < size_) {
            step_++;
            current_time_ = std::chrono::system_clock::now();
        }
        return step_;
    }

    /**
     * @brief Checks if the progress has reached the end.
     *
     * @return bool Returns true if the progress has reached the end; otherwise, false.
     */
    bool isEnd() const
    {
        return step_ >= size_;
    }

    /**
     * @brief Replace {key} in the format string with the given value.
     *
     * @param key The key to set.
     * @param value The value to set for the key.
     *
     * @return Tqdm& A reference to the Tqdm object.
     */
    void set(const std::string& key, const std::string& value)
    {
        data_[key] = value;
    }

    // getters and setters
    const std::string& getFormat() const { return format_; }
    void setFormat(const std::string& format) { format_ = format; }
    size_t getSize() const { return size_; }
    void setSize(size_t size) { size_ = size; }
    int getMinInterval() const { return mininterval_; }
    void setMinInterval(int mininterval) { mininterval_ = mininterval; }
    int getWidth() const { return width_; }
    void setWidth(int width) { width_ = width; }
    const std::vector<std::string>& getProgressPatterns() const { return progress_bar_.getPatterns(); }
    void setProgressPatterns(std::vector<std::string> patterns) { progress_bar_.setPatterns(std::move(patterns)); }

    std::string toString()
    {
        // calculate estimated, remaining, passed time, and progress
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_ - start_time_).count();
        auto estimated_time = step_ ? static_cast<int>(int64_t(cost) * size_ / step_) : 0l;
        auto remaining_time = step_ ? static_cast<int>(int64_t(cost) * (size_ - step_) / step_) : 0l;
        auto passed_time = step_ ? static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(current_time_ - start_time_).count()) : 0l;
        double progress = static_cast<double>(step_) / size_;

        // split format string by "{pbar}"
        std::string first_half, second_half;
        size_t pos = format_.find("{pbar}");
        bool has_pbar = pos != std::string::npos;
        if (has_pbar) {
            first_half = format_.substr(0, pos);
            second_half = format_.substr(pos + 6);
        } else {
            first_half = format_.substr(0, pos);
            second_half = "";
        }

        // parse format string
        std::vector<Match> matches[2] = {parseFormatString(first_half), parseFormatString(second_half)};

        // get time format of estimated, remaining, and passed time
        std::string time_format[3] = {"h:m:s", "h:m:s", "h:m:s"}; // estimated, remaining, passed
        constexpr const char* time_key[3] = {"es", "re", "ps"};
        for (auto& match : matches) {
            // find the data of {"es", "re", "ps"} in format string
            for (int i = 0; i < 3; ++i) {
                auto it = std::find_if(match.begin(), match.end(), [&time_key, i](const Match& m) { return m.fmt.key == time_key[i]; });
                if (it != match.end()) {
                    time_format[i] = it->fmt.data;
                }
            }
        }

        // replace format string with given data
        data_["percentage"] = std::to_string(static_cast<int>(progress * 100)) + "%";
        data_["step"] = std::to_string(step_);
        data_["size"] = std::to_string(size_);
        data_["es"] = secondsToTimeString(time_format[0], estimated_time / 1000);
        data_["re"] = secondsToTimeString(time_format[1], remaining_time / 1000);
        data_["ps"] = secondsToTimeString(time_format[2], passed_time / 1000);
        std::string first_half_output = formatString(first_half, matches[0], data_);
        std::string second_half_output = formatString(second_half, matches[1], data_);

        // calculate progress bar width
        // if width is set to 0, the width will be automatically set to the width of the terminal
        if (has_pbar) {
            int bar_width = (width_ ? width_ : getTerminalWidth()) - first_half_output.size() - second_half_output.size();
            if (bar_width < 0) { bar_width = 0; }
            progress_bar_.setWidth(bar_width);
            progress_bar_.setPercentage(progress);
        }

        std::string result = std::move(first_half_output);
        if (has_pbar) { result += progress_bar_.toString(); }
        result += second_half_output;
        return result;
    }

    /**
     * @brief Overloading the << operator to display the progress bar, estimated time, and remaining time.
     *
     * @param os The output stream where the progress bar will be displayed.
     * @param tqdm The Tqdm object representing the progress bar.
     *
     * @return std::ostream& The output stream containing the progress bar and time information.
     */
    friend std::ostream& operator<<(std::ostream& os, Tqdm& tqdm)
    {
        auto current_print_time = std::chrono::system_clock::now();
        if (!tqdm.first_print_ && !tqdm.isEnd() && std::chrono::duration_cast<std::chrono::milliseconds>(current_print_time - tqdm.last_print_time_).count() < tqdm.mininterval_) {
            return os;
        }
        tqdm.last_print_time_ = current_print_time;
        tqdm.first_print_ = false;

        return os << '\r' << tqdm.toString();
    }

private:
    static std::string secondsToTimeString(const std::string& format, std::time_t seconds)
    {
        std::string str;
        std::time_t day = seconds / 86400;
        std::time_t hour = (seconds % 86400) / 3600;
        std::time_t minute = (seconds % 3600) / 60;
        std::time_t second = seconds % 60;
        // parse format
        std::string time_string;
        bool is_escape = false;
        for (size_t i = 0; i < format.length(); ++i) {
            if (is_escape) {
                time_string += format.at(i);
                is_escape = false;
            } else {
                switch (format.at(i)) {
                    case 'd':
                    case 'D': time_string += intToString(day, 2); break;
                    case 'h':
                    case 'H': time_string += intToString(hour, 2); break;
                    case 'm':
                    case 'M': time_string += intToString(minute, 2); break;
                    case 's':
                    case 'S': time_string += intToString(second, 2); break;
                    case '\\': is_escape = true; break;
                    default: time_string += format.at(i); break;
                }
            }
        }
        return time_string;
    }

    static std::string intToString(int value, int width = 0)
    {
        char buf[16];
        constexpr char zero_fill_format[] = "%0*d", non_zero_fill_format[] = "%*d";

        if (width > 15) { width = 15; }
        if (width < 0) { width = 0; }

        auto format = width ? zero_fill_format : non_zero_fill_format;
        snprintf(buf, sizeof(buf), format, width, value);
        return buf;
    }

private:
    size_t size_;
    std::string format_;
    int mininterval_;
    int width_;
    size_t step_;
    bool first_print_;
    TimeType start_time_;
    TimeType current_time_;
    TimeType last_print_time_;
    ProgressBar progress_bar_;
    std::unordered_map<std::string, std::string> data_;
};

template <class IteratorType>
class TqdmWrapper : public IteratorHook<IteratorType> {
public:
    TqdmWrapper(const IteratorType& begin_it, const IteratorType& end_it, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", std::ostream& os = std::cerr, int mininterval = 100, int width = 0)
        : IteratorHook<IteratorType>(begin_it, end_it, [this](IteratorType it) {
              if (it == this->end_it_) {
                  if (os_) { *os_ << tqdm_ << std::endl; }
                  return;
              }
              if (os_) { *os_ << tqdm_; }
              tqdm_.step();
          }),
          tqdm_(end_it - begin_it, format, mininterval, width), os_(&os) {}

    TqdmWrapper(const IteratorType& begin_it, size_t size, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", std::ostream& os = std::cerr, int mininterval = 100, int width = 0)
        : IteratorHook<IteratorType>(
              begin_it, [&] {auto end_it = begin_it; std::advance(end_it, size); return end_it; }(), [this](IteratorType it) {
              if (it == this->end_it_) {
                  if (os_) { *os_ << tqdm_ << std::endl; }
                  return;
              }
              if (os_) { *os_ << tqdm_; }
              tqdm_.step(); }),
          tqdm_(size, format, mininterval, width), os_(&os)
    {
    }

    template <class ContainerType>
    TqdmWrapper(ContainerType&& container, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", std::ostream& os = std::cerr, int mininterval = 100, int width = 0)
        : IteratorHook<IteratorType>(container.begin(), container.end(), [](IteratorType) {}),
          tqdm_(container.size(), format, mininterval, width), os_(&os)
    {
        this->hook_ = [this, container = std::forward<ContainerType>(container)](IteratorType it) {
            if (it == this->end_it_) {
                if (os_) { *os_ << tqdm_ << std::endl; }
                return;
            }
            if (os_) { *os_ << tqdm_; }
            tqdm_.step();
        };
    }

    /**
     * @brief Replace {key} in the format string with the given value.
     *
     * @param key The key to set.
     * @param value The value to set for the key.
     */
    void set(const std::string& key, const std::string& value)
    {
        tqdm_.set(key, value);
    }

    // getters and setters
    const std::string& getFormat() const { return tqdm_.getFormat(); }
    void setFormat(const std::string& format) { tqdm_.setFormat(format); }
    size_t getSize() const { return tqdm_.getSize(); }
    void setSize(size_t size) { tqdm_.setSize(size); }
    void setOstream(std::ostream& os) { os_ = &os; }
    int getMinInterval() const { return tqdm_.getMinInterval(); }
    void setMinInterval(int mininterval) { tqdm_.setMinInterval(mininterval); }
    int getWidth() const { return tqdm_.getWidth(); }
    void setWidth(int width) { tqdm_.setWidth(width); }
    const std::vector<std::string>& getProgressPatterns() const { return tqdm_.getProgressPatterns(); }
    void setProgressPatterns(std::vector<std::string> patterns) { tqdm_.setProgressPatterns(std::move(patterns)); }

    std::string toString() { return tqdm_.toString(); }
    friend std::ostream& operator<<(std::ostream& os, TqdmWrapper& tqdm) { return os << tqdm.tqdm_; }

private:
    Tqdm tqdm_;
    std::ostream* os_;
};

/**
 * @brief Creates an IteratorHook instance with a progress indicator using Tqdm for a range of iterators.
 *
 * This function generates an IteratorHook instance to iterate over a range of iterators and display a
 * progress bar using Tqdm, indicating the progress of operations on the specified range.
 *
 * @tparam IteratorType The type of iterators used for the range.
 * @param begin_it The starting iterator of the range.
 * @param end_it The ending iterator of the range.
 * @param format The format string for the progress bar (defaults to "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]").
 * @param os The output stream where the progress bar will be displayed (defaults to std::cerr).
 * @param mininterval The minimum interval between updates in milliseconds (defaults to 100).
 * @param width The width of the progress bar (defaults to 0, which will be set to the width of the terminal).
 *
 * @return TqdmWrapper<IteratorType> An TqdmWrapper instance managing the iteration over the iterator range with a progress indicator.
 *
 * @note The format string can include user-defined text and keywords enclosed in curly braces, such as {key}.
 * @note The following special keywords will be replaced automatically:
 * @note  - {title}: The title for the progress bar (defaults to "").
 * @note  - {percentage}: The percentage of the progress.
 * @note  - {pbar}: The progress bar.
 * @note  - {step}: The current step value.
 * @note  - {size}: The total size of the operation or iterations.
 * @note  - {es}: The estimated time for the operation to complete. Users can customize the format using additional specifiers, e.g., {es;d:h:m:s} will display in the format of "dd:hh:mm:ss".
 * @note  - {re}: The remaining time for the operation to complete. Similar to {es}, users can define custom formats and choose specific time units to display.
 * @note  - {ps}: The passed time for the operation. Like {es} and {re}, users have the flexibility to tailor the format and displayed time units as needed.
 * @note Users can define custom keywords by enclosing them in curly braces, e.g., {custom_key}.
 * @note Custom keywords can be replaced by calling set("custom_key", "value").
 */
template <class IteratorType>
TqdmWrapper<IteratorType> tqdm(const IteratorType& begin_it, const IteratorType& end_it, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", std::ostream& os = std::cerr, int mininterval = 100, int width = 0)
{
    return {begin_it, end_it, format, os, mininterval, width};
}

/**
 * @brief Creates an IteratorHook instance with a progress indicator using Tqdm for a range defined by a starting iterator and size.
 *
 * This function generates an IteratorHook instance to iterate over a range defined by a starting iterator and a specified size,
 * displaying a progress bar using Tqdm, indicating the progress of operations within that range.
 *
 * @tparam IteratorType The type of iterators used for the range.
 * @param begin_it The starting iterator of the range.
 * @param size The size of the range to iterate over.
 * @param format The format string for the progress bar (defaults to "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]").
 * @param os The output stream where the progress bar will be displayed (defaults to std::cerr).
 * @param mininterval The minimum interval between updates in milliseconds (defaults to 100).
 * @param width The width of the progress bar (defaults to 0, which will be set to the width of the terminal).
 *
 * @return TqdmWrapper<IteratorType> An TqdmWrapper instance managing the iteration over the defined range with a progress indicator using Tqdm.
 *
 * @note The format string can include user-defined text and keywords enclosed in curly braces, such as {key}.
 * @note The following special keywords will be replaced automatically:
 * @note  - {title}: The title for the progress bar (defaults to "").
 * @note  - {percentage}: The percentage of the progress.
 * @note  - {pbar}: The progress bar.
 * @note  - {step}: The current step value.
 * @note  - {size}: The total size of the operation or iterations.
 * @note  - {es}: The estimated time for the operation to complete. Users can customize the format using additional specifiers, e.g., {es;d:h:m:s} will display in the format of "dd:hh:mm:ss".
 * @note  - {re}: The remaining time for the operation to complete. Similar to {es}, users can define custom formats and choose specific time units to display.
 * @note  - {ps}: The passed time for the operation. Like {es} and {re}, users have the flexibility to tailor the format and displayed time units as needed.
 * @note Users can define custom keywords by enclosing them in curly braces, e.g., {custom_key}.
 * @note Custom keywords can be replaced by calling set("custom_key", "value").
 */
template <class IteratorType>
TqdmWrapper<IteratorType> tqdm(const IteratorType& begin_it, size_t size, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", std::ostream& os = std::cerr, int mininterval = 100, int width = 0)
{
    auto end_it = begin_it;
    std::advance(end_it, size);
    return {begin_it, end_it, format, os, mininterval, width};
}

/**
 * @brief A function emulating the behavior of Python's tqdm module to display progress for containers.
 *
 * This function generates an IteratorHook instance to iterate over the elements of a container and
 * display a progress bar using Tqdm. It allows tracking progress of operations on the container.
 *
 * @tparam ContainerType The type of the container (vector, list, etc.) being iterated.
 * @param container The container over which to iterate.
 * @param format The format string for the progress bar (defaults to "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]").
 * @param os The output stream where the progress bar will be displayed (defaults to std::cerr).
 * @param mininterval The minimum interval between updates in milliseconds (defaults to 100).
 * @param width The width of the progress bar (defaults to 0, which will be set to the width of the terminal).
 *
 * @return TqdmWrapper<typename ContainerType::iterator> An TqdmWrapper instance managing the iteration over the container elements with a progress indicator.
 *
 * @note The format string can include user-defined text and keywords enclosed in curly braces, such as {key}.
 * @note The following special keywords will be replaced automatically:
 * @note  - {title}: The title for the progress bar (defaults to "").
 * @note  - {percentage}: The percentage of the progress.
 * @note  - {pbar}: The progress bar.
 * @note  - {step}: The current step value.
 * @note  - {size}: The total size of the operation or iterations.
 * @note  - {es}: The estimated time for the operation to complete. Users can customize the format using additional specifiers, e.g., {es;d:h:m:s} will display in the format of "dd:hh:mm:ss".
 * @note  - {re}: The remaining time for the operation to complete. Similar to {es}, users can define custom formats and choose specific time units to display.
 * @note  - {ps}: The passed time for the operation. Like {es} and {re}, users have the flexibility to tailor the format and displayed time units as needed.
 * @note Users can define custom keywords by enclosing them in curly braces, e.g., {custom_key}.
 * @note Custom keywords can be replaced by calling set("custom_key", "value").
 */
template <class ContainerType>
auto tqdm(ContainerType&& container, const std::string& format = "{title}{percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]", std::ostream& os = std::cerr, int mininterval = 100, int width = 0)
{
    return TqdmWrapper<decltype(container.begin())>(std::forward<ContainerType>(container), format, os, mininterval, width);
}

} // namespace minizero::utils::tqdm
