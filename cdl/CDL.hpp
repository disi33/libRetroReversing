#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../include/libRR.h"

#ifndef __has_include
  static_assert(false, "__has_include not supported");
#else
#  if __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
     namespace fs = std::experimental::filesystem;
#  elif __has_include(<filesystem>)
#    include <filesystem>
     namespace fs = std::fs::filesystem;
#  elif __has_include(<boost/filesystem.hpp>)
#    include <boost/filesystem.hpp>
     namespace fs = boost::filesystem;
#  endif
#endif

using namespace std;
unsigned int hex_to_int(string str);
extern "C" {
    void printBytes(uint8_t* mem, uint32_t cartAddr, uint32_t length);
    void printWords(uint8_t* mem, uint32_t cartAddr, uint32_t length);
    double jaro_winkler_distance(const char *s, const char *a);
}

string printBytesToDecimalJSArray(uint8_t* mem, uint32_t length=0x18, bool swapEndian = false);

void printf_endian_swap(const char* data);
string string_endian_swap(const char* data);
string alphabetic_only_name(char* mem, int length);
string get_header_ascii(uint8_t* mem, uint32_t proper_cart_address);
string printBytesToStr(uint8_t* mem, uint32_t length, bool swapEndian) ;
string printBytesToStr(uint8_t* mem, uint32_t length) ;
string printWordsToStr(uint8_t* mem, uint32_t length);
void save_dram_rw_to_json();
void readJsonToObject(string filename, json& json_object, string default_json="{}");
void saveJsonToFile(string filename, json& json_object);

#define Swap4Bytes(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
(((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )

// 
// Web structures
// 
struct player_settings {
    bool paused;
    bool playbackLogged;
    bool recordInput;
    bool fullLogging;
    int endAt; // frame to end the current playthought
    int loopFrame;
};
void to_json(json& j, const player_settings& p);
void from_json(const json& j, player_settings& p);
extern player_settings libRR_settings;

struct libRR_paths {
    string retro_save_directory;
    string retro_base_directory;
    string retro_cd_base_directory;
    string retro_cd_path;
    // string retro_cd_base_name;
};

// Current state send back to web
struct libRR_emulator_state {
    string game_name;
    retro_system_av_info libretro_video_info;
    retro_system_info libretro_system_info;
    retro_game_info libretro_game_info;
    std::vector<retro_memory_descriptor> memory_descriptors;
    libRR_paths paths;
    std::vector<libRR_save_state> libRR_save_states;
    // libRR_save_state current_state;
    // int last_frame;
};
void to_json(json& j, const libRR_emulator_state& p);
// void from_json(const json& j, libRR_emulator_state& p);

void to_json(json& j, const retro_system_av_info& p);
// void from_json(const json& j, retro_system_av_info& p);
void to_json(json& j, const retro_memory_map& p);
void to_json(json& j, const retro_memory_descriptor& p);
void to_json(json& j, const libRR_paths& p);

// 
// N64 structures
// 
struct cdl_tlb {
    uint32_t start;
    uint32_t end;
    uint32_t rom_offset;
};
struct cdl_memory_map {
    uint32_t start;
    uint32_t end;
    string type;
};
struct cdl_dma {
    uint32_t dram_start;
    uint32_t dram_end;
    uint32_t rom_start;
    uint32_t rom_end;
    uint32_t length;
    uint32_t header;
    uint32_t frame;
    uint32_t tbl_mapped_addr;
    string func_addr;
    bool is_assembly;
    string ascii_header;
    string guess_type;
    string known_name;
};
struct cdl_dram_cart_map {
    string dram_offset;
    string rom_offset;
};
struct cdl_playthrough_function {
    uint32_t first_frame_access;
    uint32_t last_frame_access;
    uint32_t number_of_calls;
    uint32_t number_of_frames; // number of frames that use this function
};
struct cdl_labels {
    string func_offset;
    string caller_offset;
    string func_name;
    string stack_trace;
    uint32_t func_stack;
    uint32_t return_offset_from_start; // max return offset
    string function_bytes;
    string function_bytes_endian;
    // uint32_t parameter_count;
    std::map<uint32_t, string> function_calls;
    std::map<string, string> read_addresses;
    std::map<string, string> write_addresses;
    std::map<string, string> printfs;
    std::map<int32_t, string> notes;
    bool isRenamed;
    bool doNotLog;
    bool many_memory_writes;
    bool many_memory_reads;
    bool generatedSignature;
    json additional;
    string export_path;
    string bank_number="";
    uint32_t bank_offset; // offset without taking into account bank switching
};
struct cdl_jump_return {
    string func_offset;
    string return_offset;
    string caller_offset;
};

void to_json(json& j, const cdl_jump_return& p);
void from_json(const json& j, cdl_jump_return& p);

void to_json(json& j, const cdl_labels& p);
void from_json(const json& j, cdl_labels& p);

void to_json(json& j, const cdl_dram_cart_map& p);
void from_json(const json& j, cdl_dram_cart_map& p);

void to_json(json& j, const cdl_dma& p);
void from_json(const json& j, cdl_dma& p);

void to_json(json& j, const cdl_tlb& p);
void from_json(const json& j, cdl_tlb& p);

// 
// # Save states
// 
void to_json(json& j, const libRR_save_state& p);
void from_json(const json& j, libRR_save_state& p);

template <typename I> std::string n2hexstr(I w, size_t hex_len = sizeof(I)<<1) {
    static const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len,'0');
    for (size_t i=0, j=(hex_len-1)*4 ; i<hex_len; ++i,j-=4)
        rc[i] = digits[(w>>j) & 0x0f];
    return rc;
}

string to_hex(int my_integer);
string buffer_to_string(void* my_buf);

extern std::map<string, string> function_signatures;

extern std::map<uint32_t, cdl_dram_cart_map> audio_samples;
extern std::map<uint32_t, cdl_dram_cart_map> cart_rom_dma_writes;
extern std::map<uint32_t, cdl_dram_cart_map> dma_sp_writes;
extern std::map<uint32_t, cdl_labels> labels;
extern std::map<uint32_t, cdl_labels> functions;
extern std::map<uint32_t, cdl_jump_return> jump_returns;
extern std::map<uint32_t,cdl_tlb> tlbs;
extern std::map<uint32_t,cdl_dma> dmas;

// Console specific overrides
extern "C" {
void console_log_jump_return(int take_jump, uint32_t jump_target, uint32_t pc, uint32_t ra, int64_t* registers, void* r4300);

void main_state_save(int format, const char *filename);
void main_state_load(const char *filename);
void show_interface();
void corruptBytes(uint8_t* mem, uint32_t cartAddr, int times);
void saveJsonToFile();
void write_rom_mapping();
void cdl_log_pif_ram(uint32_t address, uint32_t* value);
void find_asm_sections();
void find_audio_sections();
void find_audio_functions();
bool isAddressCartROM(uint32_t address);
void add_tag_to_function(string tag, uint32_t labelAddr);
uint32_t map_assembly_offset_to_rom_offset(uint32_t assembly_offset, uint32_t tlb_mapped_addr);
string print_function_stack_trace();
bool is_auto_generated_function_name(string func_name);
uint16_t two_bytes_to_16bit_value(uint8_t byte1, uint8_t byte2);
uint32_t three_bytes_to_24bit_value(uint8_t byte1, uint8_t byte2, uint8_t byte3);
uint32_t four_bytes_to_32bit_value(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);

extern int   l_CurFrame;
extern string rom_name;

// Internal webserver
extern void setup_web_server();
extern void stop_web_server();
}