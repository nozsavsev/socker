#include "socker.h"

static_assert(SOCKET_ERROR <= 0, "Unexpected SOCKET_ERROR code");
static_assert(sizeof(size_t) >= 4, "Unexpected size_t size");

namespace socker
{
#pragma region Smart_buff_t


    smart_buff_t::smart_buff_t(size_t size_param, int erase_val)
    {
        mem = NULL;
        actual_data_size = 0;
        memblock_size = 0;
        position = 0;

        if (size_param)
        {
            memblock_size = size_param;
            mem = (char*)malloc(memblock_size);

            if (mem)//OK
            {
#ifdef _DEBUG
                memset(mem, erase_val, memblock_size);
#endif // DEBUG
            }
            else//err
            {
                memblock_size = 0;
            }
        }
    }

    smart_buff_t::smart_buff_t(const smart_buff_t& source)
    {
        mem = NULL;
        actual_data_size = 0;
        memblock_size = 0;
        position = 0;

        Write_Over(source);
        position = source.position;
    }

    smart_buff_t::~smart_buff_t()
    {
        if (mem)
        {
            free(mem);
            mem = NULL;
        };

        mem = NULL;
        actual_data_size = 0;
        memblock_size = 0;
        position = 0;

    }

    smart_buff_t& smart_buff_t::operator=(const smart_buff_t& src)
    {
        Write_Over(src, true);
        position = src.position;

        return (*this);
    }

    bool smart_buff_t::operator==(smart_buff_t oth)
    {
        if (oth.actual_data_size != this->actual_data_size)
            return false;

        if (!memcmp(oth.mem, mem, this->actual_data_size))
            return true;

        return false;
    }

    bool smart_buff_t::Append(void* source, size_t src_size)
    {
        if (actual_data_size - position < src_size)  //if we do not enough memory 
            if (!Resize(actual_data_size + src_size))//resize
                return false;                        //resize check

        memcpy((char*)mem + position, source, src_size);

        position += src_size;

        return true;
    }

    bool smart_buff_t::Append(smart_buff_t source)
    {
        return Append(source.mem, source.actual_data_size);
    }

    bool smart_buff_t::Resize(size_t new_size, int erase_val)
    {
        if (new_size == memblock_size)
            return true;

        if (mem == nullptr) //if mem is NULL allocate mem
        {
            mem = (char*)malloc(new_size);

            if (mem) //OK
            {
                memblock_size = new_size;
#ifdef _DEBUG
                memset(mem, erase_val, memblock_size);
#endif
                return true;
            }

            else //err
            {
                memblock_size = 0;
                return false;
            }
        }

        if (new_size == 0)
        {
            free(mem);
            mem = 0;
            memblock_size = 0;
            return true;
        }

        void* temp_mem = realloc(mem, new_size);

        if (temp_mem)
        {
#ifdef _DEBUG
            if (new_size > memblock_size)//erase new mem with zero
                memset((char*)temp_mem + memblock_size, 0, new_size - memblock_size);
#endif
            mem = (char*)temp_mem;

            memblock_size = new_size;

            return true;
        }

        return false;
    }

    bool smart_buff_t::Write_Over(void* source, size_t src_size, size_t offset, bool reduce_if_possible)
    {
        size_t size_tmp = memblock_size - offset;

        if (src_size > size_tmp || reduce_if_possible)
            if (!this->Resize(offset + src_size))
                return false;

        memcpy(((char*)mem + offset), source, src_size);

        position = (offset + src_size);
        actual_data_size = (offset + src_size);

        return true;
    }

    bool smart_buff_t::Write_Over(smart_buff_t source, size_t offset, bool reduce_if_possible)
    {
        return Write_Over(source.mem, source.actual_data_size, offset, reduce_if_possible);
    }

    inline size_t smart_buff_t::GetPosition()
    {
        return position;
    }

    inline bool smart_buff_t::SetPosition(size_t position_param)
    {
        if (position_param <= position)
        {
            position = position_param;
            return true;
        }
        return false;
    }


#pragma endregion



#pragma region Send
    int Send(SOCKET socket, smart_buff_t* tx_buffer, void (*progress_callback)(size_t current, size_t all), size_t call_frec)
    {
        static size_t package_prescaler = MPCK_DEFAULT_PRESCALER;

        if (tx_buffer == nullptr)
            return SR_BADBUFF;

        size_t whole_transaction_size = tx_buffer->Get_DataSize();

        if (::send(socket, (char*)&whole_transaction_size, 8, 0) <= 0)
            return SR_BADSOCK;

        char state;
        if (::recv(socket, &state, 1, MSG_WAITALL) > 0)
        {
            if (state == OK_STATE)
            {
                int sn_retval = 0;
                size_t oneTime_transaction_size = whole_transaction_size / package_prescaler;
                if (!oneTime_transaction_size)
                    oneTime_transaction_size = whole_transaction_size;

                size_t rem_size = whole_transaction_size;

                char* tx_mem = (char*)tx_buffer->Get_Raw_Mem();

                call_frec = whole_transaction_size / call_frec;
                size_t innaccuracy = call_frec / 20LL;

                while (rem_size)
                {
                    if (rem_size < oneTime_transaction_size)
                        oneTime_transaction_size = rem_size;

                    if ((sn_retval = ::send(socket, tx_mem, oneTime_transaction_size, 0)) <= 0)
                    {
                        if (progress_callback != nullptr)
                            progress_callback(whole_transaction_size - rem_size, whole_transaction_size);
                        return SR_BADSOCK;
                    }

                    tx_mem += sn_retval;
                    rem_size -= sn_retval;

                    if (progress_callback != nullptr && (int)(rem_size % call_frec) <= innaccuracy)
                        progress_callback(whole_transaction_size - rem_size, whole_transaction_size);
                }
            }
            else
                return SR_SERVER_MALL_FAL;
        }

        return SR_OK;
    }

#pragma endregion

#pragma region Recieve

    int Receive(SOCKET socket, smart_buff_t* rx_buffer, bool append, void (*progress_callback)(size_t current, size_t all), size_t call_frec)
    {
        static size_t package_prescaler = MPCK_DEFAULT_PRESCALER;
        static size_t innaccuracy = call_frec / CALLBACK_FREQ_INNACCURACY;

        if (rx_buffer == nullptr)
            return SR_BADBUFF;


        size_t whole_transaction_size = 0;
        if (::recv(socket, (char*)&whole_transaction_size, 8, MSG_WAITALL) < 0)//recieve transaction size
            return SR_BADSOCK;


        {//resize buffer
            size_t target_size = whole_transaction_size;
            if (append)
                target_size += rx_buffer->Get_DataSize();

            if (target_size > rx_buffer->Get_MemSize())
                rx_buffer->Resize(target_size);
        }

        if (rx_buffer->Get_MemSize() >= whole_transaction_size)//check rx_buffer size
        {
            if (::send(socket, &OK_STATE, 1, 0) <= 0)
                return SR_BADSOCK;
        }
        else
        {
            ::send(socket, &NO_STATE, 1, 0);
            return SR_MALLFAL;
        }

        {

            int rc_retval = 0;     //recieve retval
            size_t rem_size = whole_transaction_size;

            char* rx_mem = (char*)rx_buffer->Get_Raw_Mem();//get memory pointer
            if (append)
                rx_mem += rx_buffer->Get_DataSize();

            size_t oneTime_transaction_size = whole_transaction_size / package_prescaler;//recv packet size
            if (!oneTime_transaction_size)
                oneTime_transaction_size = whole_transaction_size;

            while (rem_size)//have data to recieve
            {
                if (rem_size < oneTime_transaction_size)//check recv size
                    oneTime_transaction_size = rem_size;

                if ((rc_retval = ::recv(socket, rx_mem, oneTime_transaction_size, MSG_WAITALL)) <= 0)
                {
                    rem_size -= rc_retval;
                    if (progress_callback != nullptr && (int)(rem_size % call_frec) <= innaccuracy)
                        progress_callback(whole_transaction_size - rem_size, whole_transaction_size);
                    return SR_BADSOCK;
                }

                rx_mem += rc_retval;//move pointer
                rem_size -= rc_retval;//update remining data size

                if (progress_callback != nullptr)
                    progress_callback(whole_transaction_size - rem_size, whole_transaction_size);
            }

            if (append)
                rx_buffer->Set_DataSize(rx_buffer->Get_DataSize() + whole_transaction_size);
            else
                rx_buffer->Set_DataSize(whole_transaction_size);
        }

        return SR_OK;
    }
#pragma endregion
};
