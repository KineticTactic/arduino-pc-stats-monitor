#include <windows.h>
#include <vector>
#include <iostream>
#include <winternl.h>

#pragma comment(lib, "Ntdll.lib")

typedef struct
_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R;

static long long toInteger(LARGE_INTEGER const& integer)
{
#ifdef INT64_MAX // Does the compiler natively support 64-bit integers?
    return integer.QuadPart;
#else
    return (static_cast<long long>(integer.HighPart) << 32) | integer.LowPart;
#endif
}

class CPU
{
public:
    uint64_t prev_idle = 0;
    uint64_t prev_ker = 0;
    uint64_t prev_user = 0;
    uint64_t cur_idle = 0;
    uint64_t cur_ker = 0;
    uint64_t cur_user = 0;

    double get()
    {
        SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R* a = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R[4];
        // 4 is the total of CPU (4 cores)
        NtQuerySystemInformation(SystemProcessorPerformanceInformation, a, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R) * 4, NULL);

        prev_idle = cur_idle;
        prev_ker = cur_ker;
        prev_user = cur_user;

        cur_idle = 0;
        cur_ker = 0;
        cur_user = 0;

        // 4 is the total of CPU (4 cores)
        // Sum up the SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R array so I can get the utilization from all of the CPU
        for (int i = 0; i < 4; ++i)
        {
            SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_R b = a[i];
            cur_idle += toInteger(b.IdleTime);
            cur_ker += toInteger(b.KernelTime);
            cur_user += toInteger(b.UserTime);
        }

        //std::cout << "Cur idle " << cur_idle << '\n';
        //std::cout << "Cur ker " << cur_ker << '\n';
        //std::cout << "Cur user " << cur_user << '\n';

        uint64_t delta_idle = cur_idle - prev_idle;
        uint64_t delta_kernel = cur_ker - prev_ker;
        uint64_t delta_user = cur_user - prev_user;

        //std::cout << "Delta idle " << delta_idle << '\n';
        //std::cout << "Delta ker " << delta_kernel << '\n';
        //std::cout << "Delta user " << delta_user << '\n';

        uint64_t total_sys = delta_kernel + delta_user;
        uint64_t kernel_total = delta_kernel - delta_idle;

        delete[] a;
        // return (total_sys - delta_idle) * 100.0 / total_sys;
        return (kernel_total + delta_user) * 100.0 / total_sys;
    }
};

