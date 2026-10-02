#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

namespace mgs5vr {
// These pointers name only arguments at the verified native text entries.
// A markup context is not claimed to be a recovered menu/page owner.
struct PromptTraceOwner {
    uintptr_t node{},unit{},primary{},secondary{};
    uint32_t font{};
    bool fontKnown{};
};

// Caller holds its trace mutex. Separate finite pools ensure a typewriter
// subtitle cannot consume the input-tag diagnostics needed for a menu audit.
class PromptTraceBudget {
public:
    static constexpr size_t plainLimit=128,inputLimit=128;
    static constexpr size_t plainTextLimit=96,inputTextLimit=512;
    bool admit(std::string_view path,PromptTraceOwner owner,std::string_view source){
        const bool input=source.find("<I=G=")!=std::string_view::npos;
        if(source.empty()||source.size()>(input?inputTextLimit:plainTextLimit)
            ||source.find_first_not_of("0123456789.,:% /+-")==std::string_view::npos)return false;
        auto& pool=input?input_:plain_;
        if(pool.size()>=(input?inputLimit:plainLimit))return false;
        return pool.emplace(Record{std::string{path},owner,std::string{source}}).second;
    }
    size_t plainCount() const noexcept{return plain_.size();}
    size_t inputCount() const noexcept{return input_.size();}
private:
    struct Record {
        std::string path;
        PromptTraceOwner owner;
        std::string text;
        bool operator<(const Record& other) const noexcept {
            return std::tie(path,owner.node,owner.unit,owner.primary,owner.secondary,owner.font,owner.fontKnown,text)
                 < std::tie(other.path,other.owner.node,other.owner.unit,other.owner.primary,other.owner.secondary,
                            other.owner.font,other.owner.fontKnown,other.text);
        }
    };
    std::set<Record> plain_,input_;
};
}
