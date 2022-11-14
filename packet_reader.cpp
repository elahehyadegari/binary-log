#include <iostream>
#include <fstream>
#include <optional>
#include <iomanip>



#pragma pack(push, 1)
struct pkt_header
{
    uint16_t header;
    uint8_t id;
    uint16_t seq;
    uint8_t size;
};
#pragma pack(pop)

struct pkt
{
    pkt_header header;
    uint8_t *content;
    uint8_t checksum;
};

enum class validity_status : uint8_t
{
    in_valid_seq,
    in_valid_cs,
    in_vaild_all,
    valid
};


uint8_t check_sum(const pkt &read_pkt)
{
    uint8_t cs{0};
    cs += read_pkt.header.id;
    cs += read_pkt.header.seq;
    cs += (read_pkt.header.seq >> 8) && 0x00FF;
    cs += read_pkt.header.size;
    for (size_t i = 0; i < read_pkt.header.size; i++)
    {
        cs += read_pkt.content[i];
    }
    return cs;
}

bool read_frame(std::istream &ifs, pkt &read_pkt)
{

    auto packet_found{0};
    read_pkt.header.header = -1;
    while (ifs.good())
    {
        ifs.read((char *)&read_pkt.header, sizeof(read_pkt.header));

        if (read_pkt.header.header == 0x0000)
        {
            packet_found = 1;
            break;
        }

        //            auto back = ifs.tellg() - (std::streampos)(sizeof(read_pkt.header)-1);

        auto pos = ifs.tellg();
        pos -= sizeof(read_pkt.header) - 1;

        ifs.seekg(pos);
    }
    if (packet_found == 0)
    {
        return false;
    }

    uint8_t *content_ptr = new uint8_t[read_pkt.header.size];

    read_pkt.content = content_ptr;

    ifs.read((char *)read_pkt.content, read_pkt.header.size);
    ifs.read((char *)&read_pkt.checksum, sizeof(read_pkt.checksum));

   
    auto calc_cs{check_sum(read_pkt)};
    auto valid_cs{calc_cs == read_pkt.checksum};
   // print_pkt(read_pkt, valid_cs);
    if (valid_cs)
    {
        return true;
    }
    //todo:  not test yet
    char *_ptr = (char *)&read_pkt.header;
    auto header_found{0};
    auto found_index{0};
    for (size_t i = sizeof(read_pkt.header.header); i < sizeof(read_pkt.header) - 1; i++)
    {
        if (_ptr[i] == 0x00 && _ptr[i + 1] == 0x00)
        {
            header_found = 1;
            found_index = i;
            break;
        }
    }
    if(header_found == 0){
        if(_ptr[sizeof(read_pkt.header) - 1] == 0x00 &&read_pkt.content[0] == 0x00){
             header_found = 1;
             found_index = sizeof(read_pkt.header) ;

        }
        if(header_found == 0){
              _ptr = (char *)read_pkt.content;
  
            for (size_t i = 0; i < read_pkt.header.size - 1; i++)
            {
                if (_ptr[i] == 0x00 && _ptr[i + 1] == 0x00)
                {
                    header_found = 1;
                    found_index = sizeof(read_pkt.header) + i;
                    break;
                }
            }

        }
      

    }
    
    if(header_found){
        auto pos = ifs.tellg();
        pos -= sizeof(read_pkt.header) + read_pkt.header.size + sizeof(read_pkt.checksum) - found_index;
        ifs.seekg(pos);
        return false;

    }
    
    return true;
}

validity_status check_validity(const pkt &read_pkt, std::optional<uint16_t> &pre_seq)
{

    auto calc_cs{check_sum(read_pkt)};
    auto valid_cs{calc_cs == read_pkt.checksum};
    auto valid_seq{(!pre_seq || (pre_seq.value() + 1 == read_pkt.header.seq))};
    pre_seq = read_pkt.header.seq;

    validity_status ret_val{validity_status::in_vaild_all};
    if (valid_cs && valid_seq)
    {
        ret_val = validity_status::valid;
    }
    else if (valid_cs == 0 && valid_seq == 1)
    {
         ret_val = validity_status::in_valid_cs;
    }
    else if (valid_cs == 1 && valid_seq == 0)
    {
        ret_val =  validity_status::in_valid_seq;
    }

    return ret_val;
}

void print_pkt(const pkt &read_pkt, uint32_t pkt_num)
{

    std::cout << "PKT " << (int)pkt_num << "\t";
    std::cout << "ID=0x" << std::setw(2) << std::setfill('0') <<  std::hex << (int)read_pkt.header.id << "\t";

    std::cout << "Seq=" << std::dec << (int)read_pkt.header.seq << "\t";

    if (read_pkt.header.size)
    {
        std::cout << "Content =0x";
        for (size_t i = 0; i < read_pkt.header.size; i++)
        {
            std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)read_pkt.content[i];
        }
    }
    else
    {
        std::cout << "No Content ";
    }

    std::cout << "\n";
}

int main()
{
    pkt read_pkt;

    auto pkt_num{0};
    std::optional<uint16_t> pre_seq;

    std::ifstream in_file{"sample.bin", std::ios::binary};
    auto in_valid_cs_cnt{0};
    auto in_valid_seq_cnt{0};
    while (in_file.good())
    {
        
        if (read_frame(in_file, read_pkt))
        {

            validity_status validity;
            validity = check_validity(read_pkt, pre_seq);
            if (validity == validity_status::valid | validity == validity_status::in_valid_seq)
            {
                pkt_num++;
                
                print_pkt(read_pkt, pkt_num);
            }
            if (validity == validity_status::in_valid_cs)
            {
                in_valid_cs_cnt++;
            }
             if (validity == validity_status::in_valid_seq)
            {
                in_valid_seq_cnt++;
            }
            if(validity == validity_status::in_vaild_all)
            {
                in_valid_cs_cnt++;
                in_valid_seq_cnt++;
            }
        }
    }

    std::cout << "PKtLoss = " << in_valid_seq_cnt << "\t PktCorrupt = " << in_valid_cs_cnt << "\n";
}