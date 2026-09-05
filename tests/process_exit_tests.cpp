#include "mgs5vr/process_exit.hpp"
#include <windows.h>
#include <array>
#include <string>
#include <iostream>

namespace {
HANDLE stopEvent{},worker{},completed{};
DWORD WINAPI work(void*){WaitForSingleObject(stopEvent,INFINITE);return 0;}
void cleanup() noexcept {
    SetEvent(stopEvent);
    if(WaitForSingleObject(worker,1500)==WAIT_OBJECT_0)SetEvent(completed);
}
}
int wmain(int argc,wchar_t** argv){
    if(argc==3&&std::wstring(argv[1])==L"--child"){
        completed=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[2]);
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        worker=CreateThread(nullptr,0,work,nullptr,0,nullptr);
        if(!completed||!stopEvent||!worker)return 1;
        mgs5vr::installProcessExitHook(&cleanup);
        ExitProcess(73);
    }
    const auto name=L"Local\\MGS5VRExitTest-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    completed=CreateEventW(nullptr,TRUE,FALSE,name.c_str());
    if(!completed)return 1;
    std::array<wchar_t,32768> exe{};
    if(!GetModuleFileNameW(nullptr,exe.data(),static_cast<DWORD>(exe.size()))){CloseHandle(completed);return 1;}
    std::wstring command=L"\""+std::wstring(exe.data())+L"\" --child \""+name+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(exe.data(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)){
        CloseHandle(completed);return 1;
    }
    const bool exited=WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0;
    DWORD code{};GetExitCodeProcess(child.hProcess,&code);
    const bool cleaned=WaitForSingleObject(completed,0)==WAIT_OBJECT_0;
    if(!exited){TerminateProcess(child.hProcess,99);WaitForSingleObject(child.hProcess,2000);}
    CloseHandle(child.hThread);CloseHandle(child.hProcess);CloseHandle(completed);
    std::cout<<"Child exit signaled="<<exited<<", cleanup joined worker="<<cleaned<<", original exit code="<<code<<"\n";
    return exited&&cleaned&&code==73?0:1;
}
