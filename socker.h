#pragma once

#include <stdint.h>

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <future>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

const char OK_STATE = 'O';
const char NO_STATE = 'N';

#define MPCK_DEFAULT_PRESCALER 512LL 
#define CALLBACK_FREQ_INNACCURACY 20LL 

#define SR_BADSOCK 1
#define SR_BADBUFF 1
#define SR_MALLFAL 2
#define SR_SERVER_MALL_FAL 3
#define SR_OK      4 

namespace socker
{
    class smart_buff_t
    {
    protected:

        size_t position;

        size_t actual_data_size;
        size_t memblock_size;
        char* mem;

    public:

        smart_buff_t(size_t size_param, int erase_val = 0);
        smart_buff_t(const smart_buff_t& source);

        ~smart_buff_t();

        smart_buff_t& operator=(const smart_buff_t& src);

        bool operator==(smart_buff_t oth);

        inline void Erase(int val = 0)
        {
            memset(mem, val, memblock_size);
            position = 0;
        }

        bool Resize(size_t new_size, int erase_val = 0);

        bool Append(void* source, size_t src_size);
        bool Append(smart_buff_t source);

        bool Write_Over(void* source, size_t src_size, size_t offset = 0, bool reduce_if_possible = false);
        bool Write_Over(smart_buff_t source, size_t offset = 0, bool reduce_if_possible = false);

        inline const void* Get_Raw_Mem() { return mem; };


        inline size_t Get_DataSize() { return actual_data_size; }
        inline void Set_DataSize(size_t new_data_size) { if (this->memblock_size >= new_data_size) actual_data_size = new_data_size; }
        inline size_t Get_MemSize() { return memblock_size; }

        inline size_t GetPosition();
        inline bool SetPosition(size_t position_param);
    };

    int Send(SOCKET socket, smart_buff_t* tx_buffer, void (*progress_callback)(size_t current, size_t all) = nullptr, size_t call_frec = 512LL);
    int Receive(SOCKET socket, smart_buff_t* rx_buffer, bool append = false, void (*progress_callback)(size_t current, size_t all) = nullptr, size_t call_frec = 512LL);
};
