#include "mgs5vr/prompt_trace_budget.hpp"
#include "mgs5vr/prompt_activation_policy.hpp"
#include <iostream>
#include <stdexcept>

using namespace mgs5vr;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    {
        const auto ordinary=promptActivationPolicy(false,false,false,true);
        require(ordinary.mapCaption&&!ordinary.inlineMarkup,"ordinary users get accepted Map captions without enabling generic markup");
        const auto experiment=promptActivationPolicy(false,false,true,true);
        require(experiment.mapCaption&&experiment.inlineMarkup,"generic diagnostic opt-in keeps accepted Map captions");
        const auto optedOut=promptActivationPolicy(true,false,true,true);
        require(!optedOut.mapCaption&&!optedOut.inlineMarkup,"explicit global environment opt-out defeats both INI paths");
        const auto optedIn=promptActivationPolicy(true,true,false,true);
        require(optedIn.mapCaption&&optedIn.inlineMarkup,"legacy environment opt-in still enables the experimental parser");
        const auto mapOnlyOff=promptActivationPolicy(false,false,true,false);
        require(!mapOnlyOff.mapCaption&&mapOnlyOff.inlineMarkup,"Map-only opt-out preserves separately requested generic parser");
        const auto bothOff=promptActivationPolicy(false,false,false,false);
        require(!bothOff.mapCaption&&!bothOff.inlineMarkup,"ordinary Map-only opt-out leaves all prompt adapters off");
        const auto independentOff=promptActivationPolicy(true,true,false,false);
        require(!independentOff.mapCaption&&independentOff.inlineMarkup,"explicit Map-only preference remains authoritative under generic environment opt-in");
        require(promptMarkupAllowed(ordinary.inlineMarkup,ordinary.mapCaption,true)
            &&!promptMarkupAllowed(ordinary.inlineMarkup,ordinary.mapCaption,false),
            "default Map product admits verified footer markup while other markup stays opt-in");
        require(!promptMarkupAllowed(bothOff.inlineMarkup,bothOff.mapCaption,true)
            &&promptMarkupAllowed(mapOnlyOff.inlineMarkup,mapOnlyOff.mapCaption,false),
            "Map opt-out and independently requested generic markup retain their own scope");
        require(promptVrOwned(false,false),"VR controls may replace their native action markup");
        require(!promptVrOwned(false,true)&&!promptVrOwned(true,false)&&!promptVrOwned(true,true),
            "native-button escape and physical gamepad ownership retain native markup even through generic parser reentry");
    }
    {
        constexpr uintptr_t footer=0x1000,node=0x8000;
        constexpr unsigned observed[]{35,36,37,38};
        for(unsigned slot=0;slot<4;++slot){
            const auto units=footer+0xd0+slot*0xa0;
            require(mapFooterRowEligible(footer,node,units,5,slot,82,observed[slot]),
                "all four rows of the actual mode82 Map capture are admitted");
            require(!mapFooterRowEligible(footer,node,units+0x20,5,slot,82,observed[slot]),
                "an adjacent text unit cannot borrow the native Map row scope");
            require(!mapFooterRowEligible(footer,node,units,5,slot,82,observed[slot]+1),
                "a different caption-table entry is not the observed Map row");
            for(const unsigned unobservedMode:{46u,47u,81u,83u})
                require(!mapFooterRowEligible(footer,node,units,5,slot,unobservedMode,observed[slot]),
                    "historically misnamed or adjacent modes cannot inherit live Map admission");
        }
        require(!mapFooterRowEligible(footer,node,footer+0xd0,5,0,13,47)
            &&!mapFooterRowEligible(footer,node,footer+0xd0,5,0,86,47),
            "Development and Rival common footers remain outside default Map promotion");
        require(!mapFooterRowEligible(footer,node,footer+0xd0,4,0,82,35)
            &&!mapFooterRowEligible(footer,node,footer+0xd0,5,4,82,35),
            "native caption count and bounded slot are required");
        require(!mapFooterRowEligible(0,node,0xd0,5,0,82,35)
            &&!mapFooterRowEligible(footer,0,footer+0xd0,5,0,82,35),
            "missing native owner or text node cannot identify a Map footer");
        constexpr auto maximum=std::numeric_limits<uintptr_t>::max();
        require(!mapFooterRowEligible(maximum-0x80,node,0x4f,5,0,82,35),
            "unit-address overflow cannot alias a low native pointer");
        require(!mapFooterRowEligible(footer,node,footer+0xd0+2*0xa0,5,2,82,38)
            &&!mapFooterRowEligible(footer,node,footer+0xd0+3*0xa0,5,3,82,37),
            "observed Switch Zoom and Zoom In/Out captions cannot exchange row ownership");
    }
    const PromptTraceOwner owner{1,2,3,4,12,true};
    {
        PromptTraceBudget budget;
        for(size_t i=0;i<PromptTraceBudget::plainLimit;++i)
            require(budget.admit("plain",owner,"Growing subtitle "+std::to_string(i)),"plain diagnostics fill bounded subtitle pool");
        require(!budget.admit("plain",owner,"Another subtitle"),"plain pool stops at its finite bound");
        require(budget.admit("markup",owner,"<I=G=DECISION> Select"),"input tag survives exhausted subtitle budget");
        require(budget.plainCount()==PromptTraceBudget::plainLimit&&budget.inputCount()==1,"independent budgets retain their own counts");
        require(!budget.admit("markup",owner,"<I=G=DECISION> Select"),"same native source is traced only once");
    }
    {
        PromptTraceBudget budget;
        for(size_t i=0;i<PromptTraceBudget::inputLimit;++i)
            require(budget.admit("markup",owner,"<I=G=DECISION> Choice "+std::to_string(i)),"input diagnostics fill their bounded pool");
        require(!budget.admit("markup",owner,"<I=G=CANCEL> More"),"input tag pool also stops at a finite bound");
        require(budget.admit("plain",owner,"Ordinary caption"),"exhausted input pool does not consume plain diagnostics");
    }
    {
        PromptTraceBudget budget;
        require(budget.admit("plain",owner,"Same caption"),"initial owner is admitted");
        for(size_t index=0;index<6;++index){
            auto changed=owner;
            switch(index){
            case 0:++changed.node;break;case 1:++changed.unit;break;
            case 2:++changed.primary;break;case 3:++changed.secondary;break;
            case 4:++changed.font;break;default:changed.fontKnown=false;break;
            }
            require(budget.admit("plain",changed,"Same caption"),"different exact native metadata is not deduplicated into another owner");
        }
        require(budget.admit("markup",owner,"Same caption"),"plain setter and parser source paths stay distinct");
        auto first=owner;first.font=1;
        auto second=owner;second.font=12;
        require(budget.admit("plain",first,"23 caption")&&budget.admit("plain",second,"3 caption"),
                "font and caption boundaries cannot collide through concatenation");
    }
    {
        PromptTraceBudget budget;
        require(!budget.admit("plain",owner,"")&&!budget.admit("plain",owner,"1280 / 720.0%"),"empty and numeric-only traces do not use either budget");
        require(!budget.admit("plain",owner,std::string(PromptTraceBudget::plainTextLimit+1,'a')),"oversized plain trace is rejected");
        std::string input="<I=G=DECISION>";
        input.append(PromptTraceBudget::inputTextLimit-input.size(),'a');
        require(budget.admit("markup",owner,input),"bounded multi-action caption can retain full source text");
        input+='a';require(!budget.admit("markup",owner,input),"oversized input trace cannot expand log bounds");
        require(budget.plainCount()==0&&budget.inputCount()==1,"rejections consume no diagnostic slots");
    }
    std::cout<<"Prompt trace independent budgets, owner identity and bounds passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
