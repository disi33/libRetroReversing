#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../cdl/CDL_FileWriting.hpp"

#include "nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;

#include "CDL.hpp"

extern json fileConfig;
extern json reConfig;
extern json playthough_function_usage;
extern json libRR_console_constants;

json libultra_signatures;
json linker_map_file;
#define USE_CDL 1;

extern std::map<uint32_t,string> memory_to_log;
extern std::map<uint32_t,char> jumps;
extern std::map<uint32_t,string> audio_address;
extern std::map<uint32_t,uint8_t> cached_jumps;
std::map<uint32_t, uint8_t*> jump_data;
extern std::map<uint32_t,uint32_t> rsp_reads;
extern std::map<uint32_t,uint32_t> rdram_reads;
std::map<uint32_t,bool> offsetHasAssembly;

extern "C" {
    // TODO: move the following includes, they are for N64
// #include "../main/rom.h"
// #include "../device/r4300/tlb.h"
// #include "../device/r4300/r4300_core.h"
// #include "../device/memory/memory.h"
// TODO: need to log input and then call input.keyDown(keymod, keysym);

// 
// # Variables
// 
string rom_name = "UNKNOWN_ROM"; // ROM_PARAMS.headername
int corrupt_start =  0xb2b77c;
int corrupt_end = 0xb2b77c;
int difference = corrupt_end-corrupt_start;

void find_most_similar_function(uint32_t function_offset, string bytes);

bool libRR_finished_boot_rom = false;
string last_reversed_address = "";
bool should_reverse_jumps = false;
bool should_change_jumps = false;
int frame_last_reversed = 0;
int time_last_reversed = 0;
string libRR_game_name = "";
string ucode_crc = "";

// string next_dma_type = "";
// uint32_t previous_function = 0;
 std::vector<uint32_t> function_stack = std::vector<uint32_t>();
 std::vector<uint32_t> previous_ra; // previous return address

std::map<uint32_t, std::map<string, string> > addresses;

uint32_t rspboot = 0;

#define NUMBER_OF_MANY_READS 40
#define NUMBER_OF_MANY_WRITES 40

// 
// # Toggles
// 
bool support_n64_prints = false;
bool cdl_log_memory = false;
bool tag_functions = false;
bool log_notes = false;
bool log_function_calls = false;
bool log_ostasks = false;
bool log_rsp = false;

void cdl_keyevents(int keysym, int keymod) {
    #ifndef USE_CDL
        return;
    #endif
    printf("event_sdl_keydown frame:%d key:%d modifier:%d \n", l_CurFrame, keysym, keymod);
    should_reverse_jumps = false;
    // S key
    if (keysym == 115) {
        printf("Lets save! \n");
        main_state_save(0, NULL);
    }
    // L Key
    if (keysym == 108) {
        printf("Lets load! \n");
        main_state_load(NULL);
    }
    // Z Key
    if (keysym == 122) {
        write_rom_mapping();
        cdl_log_memory = !cdl_log_memory;
        tag_functions = !tag_functions;
        // should_change_jumps = true;
        //should_reverse_jumps = true;
        // show_interface();
    }
}

bool createdCartBackup = false;
void backupCart() {
   // libRR_game_name = alphabetic_only_name((char*)rom_name.c_str(), 21);
    std::cout << "TODO: backup";
    createdCartBackup = true;
}
void resetCart() {
    std::cout << "TODO: reset";
}

void readLibUltraSignatures() {
    std::ifstream i("libultra.json");
    if (i.good()) {
        i >> libultra_signatures;
    } 
    if (libultra_signatures.find("function_signatures") == libultra_signatures.end()) {
            libultra_signatures["function_signatures"] = R"([])"_json;
    }
}
void saveLibUltraSignatures() {
    std::ofstream o("libultra.json");
    o << libultra_signatures.dump(1) << std::endl;
}

void setTogglesBasedOnConfig() {
    cdl_log_memory = reConfig["shouldLogMemory"];
    tag_functions = reConfig["shouldTagFunctions"];
    log_notes = reConfig["shouldLogNotes"];
    log_function_calls = reConfig["shouldLogFunctionCalls"];
    support_n64_prints = reConfig["shouldSupportN64Prints"];
    log_ostasks = reConfig["shouldLogOsTasks"];
    log_rsp = reConfig["shouldLogRsp"];
}

void readJsonFromFile() {
    readLibUltraSignatures();
    readJsonToObject("symbols.json", linker_map_file);
    readJsonToObject("./reconfig.json", reConfig);
    setTogglesBasedOnConfig();
    string filename = "./configs/";
    filename+=rom_name;
    filename += ".json";
    // read a JSON file
    if (!reConfig["startFreshEveryTime"]) {
        cout << "Reading previous game config file \n";
        readJsonToObject(filename, fileConfig);
    }
    if (fileConfig.find("jumps") == fileConfig.end()) {
                fileConfig["jumps"] = R"([])"_json;
    }
    if (fileConfig.find("tlbs") == fileConfig.end()) {
                fileConfig["tlbs"] = R"([])"_json;
    }
    if (fileConfig.find("dmas") == fileConfig.end()) {
                fileConfig["dmas"] = R"([])"_json;
    }
    if (fileConfig.find("rsp_reads") == fileConfig.end()) {
                fileConfig["rsp_reads"] = R"([])"_json;
    }
    if (fileConfig.find("rdram_reads") == fileConfig.end())
                fileConfig["rdram_reads"] = R"([])"_json;
    if (fileConfig.find("reversed_jumps") == fileConfig.end())
                fileConfig["reversed_jumps"] = R"({})"_json;
    if (fileConfig.find("labels") == fileConfig.end())
                fileConfig["labels"] = R"([])"_json;
    if (fileConfig.find("jump_returns") == fileConfig.end())
                fileConfig["jump_returns"] = R"([])"_json;
    if (fileConfig.find("memory_to_log") == fileConfig.end())
                fileConfig["memory_to_log"] = R"([])"_json;

    memory_to_log = fileConfig["memory_to_log"].get< std::map<uint32_t,string> >();
    memory_to_log[0x0E5320] = "rsp.boot";
    jumps = fileConfig["jumps"].get< std::map<uint32_t,char> >();
    tlbs = fileConfig["tlbs"].get< std::map<uint32_t,cdl_tlb> >();
    dmas = fileConfig["dmas"].get< std::map<uint32_t,cdl_dma> >();
    rsp_reads = fileConfig["rsp_reads"].get< std::map<uint32_t,uint32_t> >();
    rdram_reads = fileConfig["rdram_reads"].get< std::map<uint32_t,uint32_t> >();
    labels = fileConfig["labels"].get< std::map<uint32_t,cdl_labels> >();
    jump_returns = fileConfig["jump_returns"].get< std::map<uint32_t,cdl_jump_return> >();
}

void saveJsonToFile() {
    string filename = "./configs/";
    filename += rom_name;
    filename += ".json";
    std::ofstream o(filename);
    o << fileConfig.dump(1) << std::endl;
}

void show_interface() {
    int answer;
    std::cout << "1) Reset ROM 2) Change corrupt number ";
    std::cin >> std::hex >> answer;
    if (answer == 1) {
        std::cout << "Resetting ROM";
        resetCart();
    }
    else {
        std::cout << "Unknown command";
    }
    printf("Answer: %d \n", answer);
}

void corrupt_if_in_range(uint8_t* mem, uint32_t proper_cart_address) {
    // if (proper_cart_address >= corrupt_start && proper_cart_address <= corrupt_end) { //l_CurFrame == 0x478 && length == 0x04) { //} proper_cart_address == 0xb4015c) {
    //     printf("save_state_before\n");
    //     main_state_save(0, "before_corruption");
    //     printBytes(mem, proper_cart_address);
    //     printf("MODIFIED IT!! %#08x\n\n\n", mem[proper_cart_address+1]);
    //     corruptBytes(mem, proper_cart_address, 10);
    //     printBytes(mem, proper_cart_address);
    // }
}

void corruptBytes(uint8_t* mem, uint32_t cartAddr, int times) {
    #ifndef USE_CDL
        return;
    #endif
    if (times>difference) {
        times=difference/4;
    }
    // srand(time(NULL));  //doesn't work on windows
    printf("Corrupt Start: %d End: %d Difference: %d \n", corrupt_start, corrupt_end, difference);
    int randomNewValue = rand() % 0xFF;
    for (int i=0; i<=times; i++) {
        int randomOffset = rand() % difference;
        int currentOffset = randomOffset;
        printf("Offset: %d OldValue: %#08x NewValue: %#08x \n", currentOffset, mem[cartAddr+currentOffset], randomNewValue);
        mem[cartAddr+currentOffset] = randomNewValue;
    }
}

void cdl_log_opcode(uint32_t program_counter, uint8_t* op_address) {
    // only called in pure_interp mode
        // jump_data[program_counter] = op_address;
        // if (!labels[function_stack.back()].generatedSignature) {
        //     printf("Not generated sig yet: %#08x \n", *op_address);
        // }
    
}

int note_count = 0;
void add_note(uint32_t pc, uint32_t target, string problem) {
    if (!log_notes) return;
    if (labels[function_stack.back()].doNotLog) return;
    std::stringstream sstream;
    sstream << std::hex << "pc:0x" << pc << "-> 0x" << target;
    sstream << problem << " noteNumber:"<<note_count;
    // cout << sstream.str();
    labels[function_stack.back()].notes[pc] = sstream.str();
    note_count++;
}

uint32_t map_assembly_offset_to_rom_offset(uint32_t assembly_offset, uint32_t tlb_mapped_addr) {
    // or if its in KSEG0/1
    if (assembly_offset >= 0x80000000) {
        uint32_t mapped_offset = assembly_offset & UINT32_C(0x1ffffffc);
        // std::cout << "todo:" << std::hex << assembly_offset << "\n";
        return map_assembly_offset_to_rom_offset(mapped_offset, assembly_offset);
    }

    for(auto it = tlbs.begin(); it != tlbs.end(); ++it) {
        auto t = it->second;
        if (assembly_offset>=t.start && assembly_offset <=t.end) {
            uint32_t mapped_offset = t.rom_offset + (assembly_offset-t.start);
            return map_assembly_offset_to_rom_offset(mapped_offset, assembly_offset);
        }
    }
    for(auto it = dmas.begin(); it != dmas.end(); ++it) {
        auto& t = it->second;
        if (assembly_offset>=t.dram_start && assembly_offset <=t.dram_end) {
            uint32_t mapped_offset = t.rom_start + (assembly_offset-t.dram_start);
            t.is_assembly = true;
            t.tbl_mapped_addr = tlb_mapped_addr;
            // DMA is likely the actual value in rom
            return mapped_offset;
        }
    }
    // std::cout << "Not in dmas:" << std::hex << assembly_offset << "\n";
    // std::cout << "Unmapped: " << std::hex << assembly_offset << "\n";
    return assembly_offset;
}

uint32_t current_function = 0;
void log_dma_write(uint8_t* mem, uint32_t proper_cart_address, uint32_t cart_addr, uint32_t length, uint32_t dram_addr) {
    if (dmas.find(proper_cart_address) != dmas.end() ) 
        return;

    auto t = cdl_dma();
    t.dram_start=dram_addr;
    t.dram_end = dram_addr+length;
    t.rom_start = proper_cart_address;
    t.rom_end = proper_cart_address+length;
    t.length = length;
    t.ascii_header = get_header_ascii(mem, proper_cart_address);
    t.header = mem[proper_cart_address+3];
    t.frame = l_CurFrame;

    // if (function_stack.size() > 0 && labels.find(current_function) != labels.end()) {
    t.func_addr = print_function_stack_trace(); // labels[current_function].func_name;
    // }

    dmas[proper_cart_address] = t;

    // std::cout << "DMA: Dram:0x" << std::hex << t.dram_start << "->0x" << t.dram_end << " Length:0x" << t.length << " " << t.ascii_header << " Stack:" << function_stack.size() << " " << t.func_addr << " last:"<< function_stack.back() << "\n";
    
}

void cdl_finish_pi_dma(uint32_t a) {
    // cout <<std::hex<< "Finish PI DMA:" << a << "\n";
}
void cdl_finish_si_dma(uint32_t a) {
    cout <<std::hex<< "Finish SI DMA:" << a << "\n";
}
void cdl_finish_ai_dma(uint32_t a) {
    // cout <<std::hex<< "Finish AI DMA:" << (a & 0xffffff) << "\n";
}

void cdl_clear_dma_log() {
    // next_dma_type = "cleared";
}
void cdl_clear_dma_log2() {
    // next_dma_type = "interesting";
}

void cdl_log_cart_reg_access() {
    // next_dma_type = "bin";
    add_tag_to_function("_cartRegAccess", function_stack.back());
}

void cdl_log_dma_si_read() {
    add_tag_to_function("_dmaSiRead", function_stack.back());
}

void cdl_log_copy_pif_rdram() {
    add_tag_to_function("_copyPifRdram", function_stack.back());
}

void cdl_log_si_reg_access() {
    // COntrollers, rumble paks etc
    add_tag_to_function("_serialInterfaceRegAccess", function_stack.back());
}

void cdl_log_mi_reg_access() {
    // The MI performs the read, modify, and write operations for the individual pixels at either one pixel per clock or one pixel for every two clocks. The MI also has special modes for loading the TMEM, filling rectangles (fast clears), and copying multiple pixels from the TMEM into the framebuffer (sprites).
    add_tag_to_function("_miRegRead", function_stack.back());
}
void cdl_log_mi_reg_write() {
    // The MI performs the read, modify, and write operations for the individual pixels at either one pixel per clock or one pixel for every two clocks. The MI also has special modes for loading the TMEM, filling rectangles (fast clears), and copying multiple pixels from the TMEM into the framebuffer (sprites).
    add_tag_to_function("_miRegWrite", function_stack.back());
}

void cdl_log_pi_reg_read() {
    if (function_stack.size() > 0)
    add_tag_to_function("_piRegRead", function_stack.back());
}
void cdl_log_pi_reg_write() {
    if (function_stack.size() > 0)
    add_tag_to_function("_piRegWrite", function_stack.back());
}

void cdl_log_read_rsp_regs2() {
    add_tag_to_function("_rspReg2Read", function_stack.back());
}
void cdl_log_write_rsp_regs2() {
    add_tag_to_function("_rspReg2Write", function_stack.back());
}

void cdl_log_read_rsp_regs() {
    if (function_stack.size() > 0)
    add_tag_to_function("_rspRegRead", function_stack.back());
}
void cdl_log_write_rsp_regs() {
    if (function_stack.size() > 0)
    add_tag_to_function("_rspRegWrite", function_stack.back());
}

void cdl_log_update_sp_status() {
    if (function_stack.size() > 0)
    add_tag_to_function("_updatesSPStatus", function_stack.back());
}

void cdl_common_log_tag(const char* tag) {
    if (function_stack.size() > 0)
    add_tag_to_function(tag, function_stack.back());
}

void cdl_log_audio_reg_access() {
    // TODO speed this up with a check first
    add_tag_to_function("_audioRegAccess", function_stack.back());
}

string print_function_stack_trace() {
    if (function_stack.size() ==0 || functions.size() ==0 /*|| labels.size() ==0*/ || function_stack.size() > 0xF) {
        return "";
    }
    std::stringstream sstream;
    int current_stack_number = 0;
    for (auto& it : function_stack) {
        if (strcmp(functions[it].func_name.c_str(),"") == 0) {
            sstream << "0x" << std::hex << it<< "->";
            continue;
        }
        if (current_stack_number>0) {
            sstream << "->";
        }
        sstream << functions[it].func_name;
        current_stack_number++;
    }
    // cout << "Stack:"<< sstream.str() << "\n";
    return sstream.str();
}




void resetReversing() {
    // time_last_reversed = time(0); // doesn;t work on windows
    last_reversed_address="";
}

void save_cdl_files() {
    resetReversing();
    find_asm_sections();
    find_audio_sections();
    find_audio_functions();
    save_dram_rw_to_json();
    saveJsonToFile();
    saveLibUltraSignatures();
}

uint32_t cdl_get_alternative_jump(uint32_t current_jump) {
    if (!should_change_jumps) {
        return current_jump;
    }

    for (auto& it : linker_map_file.items()) {
        uint32_t new_jump = hex_to_int(it.key());
        cout << "it:" << it.value() << " = " << it.key() << " old:" << current_jump << " new:"<< new_jump << "\n";
        linker_map_file.erase(it.key());
        should_change_jumps = false;
        return new_jump;
    }

    return current_jump;
}

int reverse_jump(int take_jump, uint32_t jump_target) {
    // this function doesn't work on windows
    // time_t now = time(0);
    // string key = n2hexstr(jump_target);          
    // printf("Reversing jump %#08x %d \n", jump_target, jumps[jump_target]);
    // take_jump = !take_jump;
    // time_last_reversed = now;
    // frame_last_reversed=l_CurFrame;
    // last_reversed_address = key;
    // fileConfig["reversed_jumps"][key] = jumps[jump_target];
    // write_rom_mapping();
    return take_jump;
}

void cdl_log_jump_cached(int take_jump, uint32_t jump_target, uint8_t* jump_target_memory) {
    if (cached_jumps.find(jump_target) != cached_jumps.end() ) 
        return;
    cached_jumps[jump_target] = 1;
    cout << "Cached:" << std::hex << jump_target << "\n";
}

int number_of_functions = 0;
bool libRR_full_function_log = false;
bool libRR_full_trace_log = true;
int last_return_address = 0;
uint32_t libRR_call_depth = 0; // Tracks how big our stack trace is in terms of number of function calls

// We store the stackpointers in backtrace_stackpointers everytime a function gets called
uint16_t libRR_backtrace_stackpointers[0x200]; // 0x200 should be tons of function calls
uint32_t libRR_backtrace_size = 0; // Used with backtrace_stackpointers - Tracks how big our stack trace is in terms of number of function calls

extern uint32_t libRR_pc_lookahead;

string current_trace_log = "";
const int trace_messages_until_flush = 40;
int current_trace_count = 0;
bool first_trace_write = true;
void libRR_log_trace_str(string message) {
    
    if (!libRR_full_trace_log) {
        return;
    }
    current_trace_log += message + "\n";
    current_trace_count++;
    if (current_trace_count >= trace_messages_until_flush) {
        libRR_log_trace_flush();
        current_trace_count = 0;
        current_trace_log="";
    }
}
void libRR_log_trace(const char* message) {
    libRR_log_trace_str(message);
}

void libRR_log_trace_flush() {
    if (!libRR_full_trace_log) {
        return;
    }
    string output_file_path = libRR_export_directory + "trace_log.txt";
    if (first_trace_write) {
        codeDataLogger::writeStringToFile(output_file_path, current_trace_log);
        first_trace_write = false;
    } else {
        codeDataLogger::appendStringToFile(output_file_path, current_trace_log);
    }
}

// libRR_log_return_statement
// stack_pointer is used to make sure our function stack doesn't exceed the actual stack pointer
void libRR_log_return_statement(uint32_t current_pc, uint32_t return_target, uint32_t stack_pointer) {
    if (libRR_full_trace_log) {
        libRR_log_trace_str("Return:"+n2hexstr(current_pc)+"->"+n2hexstr(return_target));
    }
    // printf("libRR_log_return_statement pc:%d return:%d stack:%d\n", current_pc, return_target, 65534-stack_pointer);

    // check the integrety of the call stack
    if (libRR_call_depth < 0) {
        printf("Function seems to have returned without changing the stack, PC: %d \n", current_pc);
    }

    auto function_returning_from = function_stack.back();
    auto presumed_return_address = previous_ra.back();
    if (return_target != presumed_return_address) {
        // printf("ERROR: Presumed return: %d actual return: %d current_pc: %d\n", presumed_return_address, return_target, current_pc);
        // sometimes code manually pushes the ret value to the stack and returns
        // if so we don't want to log as a function return
        // but in the future we might want to consider making the previous jump a function call
        return;
    } else {
        libRR_call_depth--;
        // Remove from stacks
        function_stack.pop_back();
        previous_ra.pop_back();
    }

    if (!libRR_full_function_log) {
        return;
    }
    
    current_pc -= libRR_pc_lookahead; 
    string current_function = n2hexstr(function_returning_from);
    string current_pc_str = n2hexstr(current_pc);
    // printf("Returning from function: %s current_pc:%s \n", current_function.c_str(), current_pc_str.c_str());
    // string function_key = current_function;
    playthough_function_usage[current_function]["returns"][current_pc_str] = return_target;

    // Add max return to functions
    if (functions.find(function_returning_from) != functions.end() ) {
        uint32_t relative_return_pc = current_pc - function_returning_from;
        if (relative_return_pc > functions[function_returning_from].return_offset_from_start) {
            functions[function_returning_from].return_offset_from_start = relative_return_pc;
        }
    }

    // TODO: Calculate Function Signature so we can check for its name
    int length = current_pc - function_returning_from;
    string length_str = n2hexstr(length);
    playthough_function_usage[current_function]["lengths"][length_str] = length;
    if (length > 0 && length < 200) {
        if (playthough_function_usage[current_function]["signatures"].contains(length_str)) {

        } else {
            printf("Function Signature: About to get length: %d \n", length);
            playthough_function_usage[current_function]["signatures"][n2hexstr(length)] = libRR_get_data_for_function(function_returning_from, length+1, true, true);
        }
    }    

    // string bytes_with_branch_delay = printBytesToStr(jump_data[previous_function_backup], byte_len+4)+"_"+n2hexstr(length+4);
    // string word_pattern = printWordsToStr(jump_data[previous_function_backup], byte_len+4)+" L"+n2hexstr(length+4,4);
    // TODO: need to get the moment where the bytes for the function are located 
    // printf("Logged inst: %s \n", name.c_str());
}



// libRR_log_full_function_call is expensive as it does extensive logging
void libRR_log_full_function_call(uint32_t current_pc, uint32_t jump_target) {
    // Instead of using function name, we just use the location
    string function_name = /*game_name + "_func_" +*/ n2hexstr(jump_target);
    // printf("libRR_log_full_function_call Full function logging on %s \n", print_function_stack_trace().c_str());


    // This is playthough specific
    if (!playthough_function_usage.contains(function_name)) {
        // printf("Adding new function %s \n", function_name.c_str());
        playthough_function_usage[function_name] = json::parse("{}");
        playthough_function_usage[function_name]["first_frame_access"] = RRCurrentFrame;
        playthough_function_usage[function_name]["number_of_frames"]=0;
        playthough_function_usage[function_name]["last_frame_access"] = 0;
        playthough_function_usage[function_name]["number_of_calls_per_frame"] = 1;
    }
    else if (RRCurrentFrame < playthough_function_usage[function_name]["last_frame_access"]) {
        // we have already ran this frame before, probably replaying, no need to add more logging
        return;
    }

    if (RRCurrentFrame > playthough_function_usage[function_name]["last_frame_access"]) {
        playthough_function_usage[function_name]["last_frame_access"] = RRCurrentFrame;
        playthough_function_usage[function_name]["number_of_frames"]= (int)playthough_function_usage[function_name]["number_of_frames"]+1;
        playthough_function_usage[function_name]["number_of_calls_per_frame"]=0;
    } 
    else if (RRCurrentFrame == playthough_function_usage[function_name]["last_frame_access"]) {
        // we are in the same frame so its called more than once per frame
        playthough_function_usage[function_name]["number_of_calls_per_frame"]=(int)playthough_function_usage[function_name]["number_of_calls_per_frame"]+1;
    }

    // TODO: log read/writes to memory
    // TODO: calculate return and paramerters
    // TODO: find out how long the function is
}

const char* libRR_log_long_jump(uint32_t current_pc, uint32_t jump_target, const char* type) {
    // cout << "Long Jump from:" << n2hexstr(current_pc) << " to:" << n2hexstr(jump_target) << "\n";
    if (libRR_full_trace_log) {
        libRR_log_trace_str("Long Jump:"+n2hexstr(current_pc)+"->"+n2hexstr(jump_target)+" type:"+type);
    }

    string target_bank_number = "0000";
    string pc_bank_number = "0000";
    target_bank_number = n2hexstr(get_current_bank_number_for_address(jump_target), 4);

    // now we need the bank number of the function we are calling
    pc_bank_number = n2hexstr(get_current_bank_number_for_address(current_pc), 4);
    libRR_long_jumps[target_bank_number][n2hexstr(jump_target)][pc_bank_number+"::"+n2hexstr(current_pc)]=type;
    return libRR_log_jump_label(jump_target, current_pc);
}

void libRR_log_interrupt_call(uint32_t current_pc, uint32_t jump_target) {
    string pc_bank_number = "0000";
    pc_bank_number = n2hexstr(get_current_bank_number_for_address(current_pc), 4);

    // printf("Interrupt call at: %s::%s target:%s \n", pc_bank_number.c_str(), n2hexstr(current_pc).c_str(), n2hexstr(jump_target).c_str());
    libRR_long_jumps["0000"][n2hexstr(jump_target)][pc_bank_number+"::"+n2hexstr(current_pc)]=true;
}

// Restarts are very similar to calls but can only jump to specific targets and only take up 1 byte
void libRR_log_rst(uint32_t current_pc, uint32_t jump_target) {
    // for now just log it as a standard function call
    libRR_log_function_call(current_pc, jump_target, 0x00);
}

string function_name = ""; // last function name called
const char* libRR_log_function_call(uint32_t current_pc, uint32_t jump_target, uint32_t stack_pointer) {
    // TODO: find out why uncommeting the following causes a segfault
    // if (!libRR_full_function_log || !libRR_finished_boot_rom) {
    //     return;
    // }
    string bank_number = "0000";
    uint32_t calculated_jump_target = jump_target;
    if (libRR_bank_switching_available) {
        int bank = get_current_bank_number_for_address(jump_target);
        bank_number = n2hexstr(bank, 4);

        // TODO: the following might be gameboy specific
        if (jump_target >= libRR_bank_size) {
            // printf("TODO: remove this in gameboy!\n");
            calculated_jump_target = jump_target + ((bank-1) * libRR_bank_size);
        }
        // END TODO
    }

    string jump_target_str = n2hexstr(jump_target);
    function_name = "_"+bank_number+"_func_"+jump_target_str;
    libRR_called_functions[bank_number][n2hexstr(jump_target)] = function_name;
    libRR_log_trace_str("Function call: 0x"+jump_target_str);
    
    // Start Stacktrace handling
    libRR_call_depth++;
    // End Stacktrace handling
    
    last_return_address = current_pc;
    function_stack.push_back(jump_target);
    previous_ra.push_back(current_pc);
    if (libRR_full_function_log) {
        libRR_log_full_function_call(current_pc, jump_target);
    }
    if (functions.find(jump_target) != functions.end() ) {
        // We have already logged this function, so ignore for now
        return function_name.c_str();
    }
    // We have never logged this function so lets create it
    auto t = cdl_labels();
    t.func_offset = n2hexstr(calculated_jump_target);
    // if (functions.find(previous_function_backup) != functions.end()) {
    //     t.caller_offset = functions[previous_function_backup].func_name+" (ra:"+n2hexstr(ra)+")";
    // } else {
    //     t.caller_offset = n2hexstr(previous_function_backup);
    // }
    t.func_name = function_name; // /*libRR_game_name+*/"_"+bank_number+"_func_"+jump_target_str;
    //t.func_stack = function_stack.size();
    //t.export_path = "";
    //t.bank_number = bank_number;
    //t.bank_offset = jump_target;
    // t.stack_trace = print_function_stack_trace();
    t.doNotLog = false;
    t.many_memory_reads = false;
    t.many_memory_writes = false;
    // t.additional["callers"][print_function_stack_trace()] = RRCurrentFrame;
    printf("Logged new function: %s target:%d number_of_functions:%d \n", t.func_name.c_str(), jump_target, number_of_functions);
    functions[jump_target] = t;
    number_of_functions++;
    return function_name.c_str();
}

// This will be replace be libRR_log_function_call
void log_function_call(uint32_t function_that_is_being_called) {
    if (!log_function_calls) return;
    uint32_t function_that_is_calling = function_stack.back();
    if (labels.find(function_that_is_calling) == labels.end()) return;
    if (labels[function_that_is_calling].isRenamed || labels[function_that_is_calling].doNotLog) return;
    labels[function_that_is_calling].function_calls[function_that_is_being_called] = labels[function_that_is_being_called].func_name;
}

void cdl_log_jump_always(int take_jump, uint32_t jump_target, uint8_t* jump_target_memory, uint32_t ra, uint32_t pc) {
    add_note(ra-8, pc, "Call (jal)");
    previous_ra.push_back(ra);
    uint32_t previous_function_backup = function_stack.back();
    function_stack.push_back(jump_target);
    current_function = jump_target;

    if (jumps[jump_target] >3) return;
    jumps[jump_target] = 0x04;

    if (labels.find(jump_target) != labels.end() ) 
        return;
    log_function_call(jump_target);
    auto t = cdl_labels();
    string jump_target_str = n2hexstr(jump_target);
    t.func_offset = jump_target_str;
    if (labels.find(previous_function_backup) != labels.end()) {
        t.caller_offset = labels[previous_function_backup].func_name+" (ra:"+n2hexstr(ra)+")";
    } else {
        t.caller_offset = n2hexstr(previous_function_backup);
    }
    t.func_name = libRR_game_name+"_func_"+jump_target_str;
    t.func_stack = function_stack.size();
    t.stack_trace = print_function_stack_trace();
    t.doNotLog = false;
    t.many_memory_reads = false;
    t.many_memory_writes = false;
    labels[jump_target] = t;
    jump_data[jump_target] = jump_target_memory;
}
void cdl_log_jump_return(int take_jump, uint32_t jump_target, uint32_t pc, uint32_t ra, int64_t* registers, struct r4300_core* r4300) {
    uint32_t previous_function_backup = -1;
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }

    // if (previous_function_backup > ra) {
    //     // cout << std::hex << " Odd the prev function start should never be before return address ra:" << ra << " previous_function_backup:" << previous_function_backup << "\n";
    //     return;
    // }

    if (function_stack.size()>0) {
        previous_function_backup = function_stack.back();
        
    }
    else {
        add_note(pc, jump_target, "function_stack <0");
        // probably jumping from exception?
        // cout << "Missed push back?" << std::hex << jump_target << " ra" << ra << " pc:"<< pc<< "\n";
        // return;
    }

    if (jump_target == previous_ra.back()) {
        add_note(pc, jump_target, "successful return");
        
    } else {
        string problem = "Expected $ra to be 0x";
        problem += n2hexstr(previous_ra.back());
        problem += " but was:";
        problem += n2hexstr(jump_target);
        add_note(pc, jump_target, problem);
        // return;
    }
    function_stack.pop_back();
    current_function = function_stack.back();
    previous_ra.pop_back();

    console_log_jump_return(take_jump, jump_target, pc, ra, registers, r4300);

    if (jumps[jump_target] >3) return;
    jumps[jump_target] = 0x04;

    if (jump_returns.find(previous_function_backup) != jump_returns.end()) 
        {
            return;
        }
    auto t = cdl_jump_return();
    string jump_target_str = n2hexstr(jump_target);
    t.return_offset = pc;
    t.func_offset = previous_function_backup;
    t.caller_offset = jump_target;
    jump_returns[previous_function_backup] = t;

    uint64_t length = pc-previous_function_backup;
    // labels[previous_function_backup].return_offset_from_start = length;
    if (length<2) {
        return;
    }

    if (jump_data.find(previous_function_backup) != jump_data.end()) {
        uint64_t byte_len = length;
        if (byte_len > 0xFFFF) {
            byte_len = 0xFFFF;
        }
        // string bytes = printBytesToStr(jump_data[previous_function_backup], byte_len)+"_"+n2hexstr(length);
        string bytes_with_branch_delay = printBytesToStr(jump_data[previous_function_backup], byte_len+4)+"_"+n2hexstr(length+4);
        string word_pattern = printWordsToStr(jump_data[previous_function_backup], byte_len+4)+" L"+n2hexstr(length+4,4);
        labels[previous_function_backup].function_bytes = bytes_with_branch_delay;
        labels[previous_function_backup].doNotLog = true;
        labels[previous_function_backup].generatedSignature = true;
        // labels[previous_function_backup].function_bytes_endian = Swap4Bytes(bytes);
        
        // now check to see if its in the mario map
        // if (/*strcmp(game_name.c_str(),"SUPERMARIO") == 0 &&*/ linker_map_file.find( n2hexstr(previous_function_backup) ) != linker_map_file.end()) {
        //     string offset = n2hexstr(previous_function_backup);
        //     string func_name = linker_map_file[offset];
        //     cout << game_name << "It is in the map file!" << n2hexstr(previous_function_backup) << " as:" << linker_map_file[n2hexstr(previous_function_backup)] << "\n";
        //     function_signatures[bytes] = func_name;
        //     if (strcmp(func_name.c_str(),"gcc2_compiled.")==0) return; // we don't want gcc2_compiled labels
        //     libultra_signatures["function_signatures"][bytes_with_branch_delay] = func_name;
        //     labels[previous_function_backup].func_name = libultra_signatures["function_signatures"][bytes_with_branch_delay];
        //     return;
        // }
        

        // if it is a libultra function then lets name it
        if (libultra_signatures["library_signatures"].find(word_pattern) != libultra_signatures["library_signatures"].end()) {
            std::cout << "In library_signatures:" <<  word_pattern << " name:"<< libultra_signatures["library_signatures"][word_pattern] << "\n";
            labels[previous_function_backup].func_name = libultra_signatures["library_signatures"][word_pattern];
            labels[previous_function_backup].isRenamed = true;
            // return since we have already named this functions, don't need its signature to be saved
            return;
        }
        if (libultra_signatures["game_signatures"].find(word_pattern) != libultra_signatures["game_signatures"].end()) {
            // std::cout << "In game_signatures:" <<  word_pattern << " name:"<< libultra_signatures["game_signatures"][word_pattern] << "\n";
            labels[previous_function_backup].func_name = libultra_signatures["game_signatures"][word_pattern];
            // return since we have already named this functions, don't need its signature to be saved
            return;
        }
        // if it is a libultra function then lets name it
        if (libultra_signatures["function_signatures"].find(bytes_with_branch_delay) != libultra_signatures["function_signatures"].end()) {
            std::cout << "In old libultra:" <<  bytes_with_branch_delay << " name:"<< libultra_signatures["function_signatures"][bytes_with_branch_delay] << "\n";
            labels[previous_function_backup].func_name = libultra_signatures["function_signatures"][bytes_with_branch_delay];
            if (labels[previous_function_backup].func_name.find("_func_") != std::string::npos) {
                // this is a non renamed function as it was auto generated
                find_most_similar_function(previous_function_backup, word_pattern);
                libultra_signatures["game_signatures"][word_pattern] = labels[previous_function_backup].func_name;
            }
            else {
                libultra_signatures["library_signatures"][word_pattern] = labels[previous_function_backup].func_name;
                labels[previous_function_backup].isRenamed = true;
            }
            libultra_signatures["function_signatures"].erase(bytes_with_branch_delay);
            // return since we have already named this functions, don't need its signature to be saved
            return;
        }

        // if it is an OLD libultra function then lets name it (without branch delay)
        // if (libultra_signatures["function_signatures"].find(bytes) != libultra_signatures["function_signatures"].end()) {
        //     std::cout << "In OLDEST libultra:" <<  bytes << " name:"<< libultra_signatures["function_signatures"][bytes] << "\n";
        //     labels[previous_function_backup].func_name = libultra_signatures["function_signatures"][bytes];
        //     labels[previous_function_backup].isRenamed = true;
        //     libultra_signatures["function_signatures"][bytes_with_branch_delay] = labels[previous_function_backup].func_name;
        //     // delete the old non-branch delay version
        //     libultra_signatures["function_signatures"].erase(bytes);
        //     // return since we have already named this functions, don't need its signature to be saved
        //     return;
        // }

        find_most_similar_function(previous_function_backup, word_pattern);
        // cout << "word_pattern:" << word_pattern << "\n";

        if (function_signatures.find(word_pattern) == function_signatures.end()) {
            // save this new function to both libultra and trace json
            function_signatures[word_pattern] = labels[previous_function_backup].func_name;
            libultra_signatures["game_signatures"][word_pattern] = labels[previous_function_backup].func_name;
        } else {
            //function_signatures.erase(bytes);
            // function_signatures[word_pattern] = "Multiple functions";
            std::cout << "Multiple Functions for :" << *jump_data[previous_function_backup] << " len:" << length << " pc:0x"<< pc << " - 0x" << previous_function_backup << "\n";
        }
    }
}

void find_most_similar_function(uint32_t function_offset, string bytes) {
    string named_function_with_highest_distance = "";
    // string auto_generated_function_with_highest_distance = "";
    double highest_distance = 0;
    // double highest_auto_distance = 0;
    for(auto it = libultra_signatures["library_signatures"].begin(); it != libultra_signatures["library_signatures"].end(); ++it) {
        double distance = jaro_winkler_distance(bytes.c_str(), it.key().c_str());
        if (distance >= highest_distance) {
            string function_name = it.value();
            // if (!is_auto_generated_function_name(function_name)) {
                if (distance == highest_distance) {
                    named_function_with_highest_distance += "_or_";
                    named_function_with_highest_distance += function_name;
                } else {
                    highest_distance = distance;
                    named_function_with_highest_distance = function_name;
                }
            // } else {
            //     highest_auto_distance = distance;
            //     auto_generated_function_with_highest_distance=function_name;
            // }
        }
        // cout << "IT:" << it.value() << " distance:" << jaro_winkler_distance(bytes_with_branch_delay.c_str(), it.key().c_str()) << "\n";
    }
    uint32_t highest_distance_percent = highest_distance*100;
    // cout << "generated function_with_highest_distance to "<< std::hex << function_offset << " is:"<<auto_generated_function_with_highest_distance<<" with "<< std::dec << highest_auto_distance <<"%\n";
    if (highest_distance_percent>=95) {
        cout << "function will be renamed "<< std::hex << function_offset << " is:"<<named_function_with_highest_distance<<" with "<< std::dec << highest_distance_percent <<"%\n";
        labels[function_offset].func_name = named_function_with_highest_distance;
        labels[function_offset].isRenamed = true;
    } else if (highest_distance_percent>=90) {
        cout << "function_with_highest_distance to "<< std::hex << function_offset << " is:"<<named_function_with_highest_distance<<" with "<< std::dec << highest_distance_percent <<"%\n";
        labels[function_offset].func_name += "_predict_"+named_function_with_highest_distance+"_";
        labels[function_offset].func_name += (to_string(highest_distance_percent));
        labels[function_offset].func_name += "percent";
    }
}

// loop through and erse multiple functions
void erase_multiple_func_signatures() {
    // function_signatures
    // Multiple functions
}

bool is_auto_generated_function_name(string func_name) {
    if (func_name.find("_func_") != std::string::npos) {
                // this is a non renamed function as it was auto generated
                return true;
    }
    return false;
}

unsigned int find_first_non_executed_jump() {
    for(map<unsigned int, char>::iterator it = jumps.begin(); it != jumps.end(); ++it) {
        if ((it->second+0) <3) {
            return it->first;
        }
    }
    return -1;
}

int cdl_log_jump(int take_jump, uint32_t jump_target, uint8_t* jump_target_memory, uint32_t pc, uint32_t ra) {
    add_note(pc, jump_target, "jump");
    // if (previous_ra.size() > 0 && ra != previous_ra.back()) {
    //     cdl_log_jump_always(take_jump, jump_target, jump_target_memory, ra, pc);
    //     //previous_ra.push_back(ra);
    //     return take_jump;
    // }
    // if (should_reverse_jumps)
    // {
    //     time_t now = time(0);
    //     if (jumps[jump_target] < 3) {
    //         // should_reverse_jumps=false;
    //         if ( now-time_last_reversed > 2) { // l_CurFrame-frame_last_reversed >(10*5) ||
    //             take_jump = reverse_jump(take_jump, jump_target);               
    //         }
    //     } else if (now-time_last_reversed > 15) {
    //         printf("Stuck fixing %d\n", find_first_non_executed_jump());
    //         take_jump=!take_jump;
    //         main_state_load(NULL);
    //         // we are stuck so lets load
    //     }
    // }
    if (take_jump) {
        jumps[jump_target] |= 1UL << 0;
    }
    else {
        jumps[jump_target] |= 1UL << 1;
    }
    return take_jump;
}

void save_table_mapping(int entry, uint32_t phys, uint32_t start,uint32_t end, bool isOdd) {
    
    //printf("tlb_map:%d ODD Start:%#08x End:%#08x Phys:%#08x \n",entry, e->start_odd, e->end_odd, e->phys_odd);
        uint32_t length = end-start;

        auto t = cdl_tlb();
        t.start=start;
        t.end = end;
        t.rom_offset = phys;
        tlbs[phys]=t;

        string key = "";
        key+="[0x";
        key+=n2hexstr(phys);
        key+=", 0x";
        key+=n2hexstr(phys+length);
        key+="] Virtual: 0x";
        key+=n2hexstr(start);
        key+=" to 0x";
        key+=n2hexstr(end);
        if (isOdd)
        key+=" Odd";
        else
        key+=" Even";

        string value = "Entry:";
        value += to_string(entry);
        // value += " Frame:0x";
        value += n2hexstr(l_CurFrame);

        bool isInJson = fileConfig["tlb"].find(key) != fileConfig["tlb"].end();
        if (isInJson) {
            string original = fileConfig["tlb"][key];
            bool isSameValue = (strcmp(original.c_str(), value.c_str()) == 0);
            if (isSameValue) return;
            // printf("isSameValue:%d \noriginal:%s \nnew:%s\n", isSameValue, original.c_str(), value.c_str());
            return; // don't replace the original value as it is useful to match frame numbers to the mappings
        }
        fileConfig["tlb"][key] = value;
        printf("TLB %s\n", value.c_str());
}

void cdl_log_dram_read(uint32_t address) {
    
}
void cdl_log_dram_write(uint32_t address, uint32_t value, uint32_t mask) {
    
}

void cdl_log_rsp_mem(uint32_t address, uint32_t* mem,int isBootRom) {
    if (isBootRom) return;
    rsp_reads[address] = (uint32_t)*mem;
}
void cdl_log_rdram(uint32_t address, uint32_t* mem,int isBootRom) {
    //printf("RDRAM %#08x \n", address);
    if (isBootRom) return;
    rdram_reads[address] = (uint32_t)*mem;
}
void cdl_log_mm_cart_rom(uint32_t address,int isBootRom) {
    printf("Cart ROM %#08x \n", address);
}
void cdl_log_mm_cart_rom_pif(uint32_t address,int isBootRom) {
    printf("PIF? %#08x \n", address);
}

void cdl_log_pif_ram(uint32_t address, uint32_t* value) {
    #ifndef USE_CDL
        return;
    #endif
    printf("Game was reset? \n");
    if (!createdCartBackup) {
        backupCart();
        readJsonFromFile();
        function_stack.push_back(0);
    }
    if (should_reverse_jumps) {
        // should_reverse_jumps = false;
        fileConfig["bad_jumps"][last_reversed_address] = "reset";
        main_state_load(NULL);
        write_rom_mapping();
    }
}

void cdl_log_opcode_error() {
    printf("Very bad opcode, caused crash! \n");
    fileConfig["bad_jumps"][last_reversed_address] = "crash";
    main_state_load(NULL);
}

void find_asm_sections() {
    printf("finding asm in sections \n");
    for(map<unsigned int, char>::iterator it = jumps.begin(); it != jumps.end(); ++it) {
        string jump_target_str = n2hexstr(it->first);
        fileConfig["jumps_rom"][jump_target_str] =  n2hexstr(map_assembly_offset_to_rom_offset(it->first,0));
    }
}

void find_audio_sections() {
    printf("finding audio sections \n");
    for(map<uint32_t, cdl_dma>::iterator it = dmas.begin(); it != dmas.end(); ++it) {
        uint32_t address = it->second.dram_start;
        if (audio_address.find(address) == audio_address.end() ) 
            continue;
        dmas[address].guess_type = "audio";
        it->second.guess_type="audio";
    }
}


void add_tag_to_function(string tag, uint32_t labelAddr) {
    if (!tag_functions || labels[labelAddr].isRenamed) return;
    if (labels[labelAddr].func_name.find(tag) != std::string::npos) return;
    labels[labelAddr].func_name += tag;
}

void find_audio_functions() {
    printf("finding audio functions \n");
    for(map<uint32_t, cdl_labels>::iterator it = labels.begin(); it != labels.end(); ++it) {
        cdl_labels label = it->second;
        if (label.isRenamed) {
            continue; // only do it for new functions
        }
        if (label.many_memory_reads) {
            add_tag_to_function("_manyMemoryReads", it->first);
        }
        if (label.many_memory_writes) {
            add_tag_to_function("_manyMemoryWrites", it->first);
        }
        for(map<string, string>::iterator it2 = label.read_addresses.begin(); it2 != label.read_addresses.end(); ++it2) {
            uint32_t address = hex_to_int(it2->first);
            if (audio_address.find(address) != audio_address.end() ) 
            {
                cout << "Function IS audio:"<< label.func_name << "\n";
            }
            if (address>0x10000000 && address <= 0x107fffff) {
                cout << "Function accesses cart rom:"<< label.func_name << "\n";
            }
        }
        for(map<string, string>::iterator it2 = label.write_addresses.begin(); it2 != label.write_addresses.end(); ++it2) {
            uint32_t address = hex_to_int(it2->first);
            if (audio_address.find(address) != audio_address.end() ) 
            {
                cout << "Function IS audio:"<< label.func_name << "\n";
            }
            if (address>0x10000000 && address <= 0x107fffff) {
                cout << "Function IS cart rom:"<< label.func_name << "\n";
            }
        }
    }
}
bool isAddressCartROM(uint32_t address) {
    return (address>0x10000000 && address <= 0x107fffff);
}

void cdl_log_audio_sample(uint32_t saved_ai_dram, uint32_t saved_ai_length) {
    if (audio_samples.find(saved_ai_dram) != audio_samples.end() ) 
        return;
    auto t = cdl_dram_cart_map();
    t.dram_offset = n2hexstr(saved_ai_dram);
    t.rom_offset = n2hexstr(saved_ai_length);
    audio_samples[saved_ai_dram] = t;
    // printf("audio_plugin_push_samples AI_DRAM_ADDR_REG:%#08x length:%#08x\n", saved_ai_dram, saved_ai_length);
}

void cdl_log_cart_rom_dma_write(uint32_t dram_addr, uint32_t cart_addr, uint32_t length) {
    if (cart_rom_dma_writes.find(cart_addr) != cart_rom_dma_writes.end() ) 
        return;
    auto t = cdl_dram_cart_map();
    t.dram_offset = n2hexstr(dram_addr);
    t.rom_offset = n2hexstr(cart_addr);
    cart_rom_dma_writes[cart_addr] = t;
    printf("cart_rom_dma_write: dram_addr:%#008x cart_addr:%#008x length:%#008x\n", dram_addr, cart_addr, length);
}

void cdl_log_dma_sp_write(uint32_t spmemaddr, uint32_t dramaddr, uint32_t length, unsigned char *dram) {
    if (dma_sp_writes.find(dramaddr) != dma_sp_writes.end() ) 
        return;
    auto t = cdl_dram_cart_map();
    t.dram_offset = n2hexstr(dramaddr);
    t.rom_offset = n2hexstr(spmemaddr);
    dma_sp_writes[dramaddr] = t;
    // FrameBuffer RSP info
    printWords(dram, dramaddr, length);
    printf("FB: dma_sp_write SPMemAddr:%#08x Dramaddr:%#08x length:%#08x  \n", spmemaddr, dramaddr, length);
}

inline void cdl_log_memory_common(const uint32_t lsaddr, uint32_t pc) {
    

    // if (addresses.find(lsaddr) != addresses.end() ) 
    //     return;
    // addresses[lsaddr] = currentMap;
}

void cdl_log_mem_read(const uint32_t lsaddr, uint32_t pc) {
    if (!cdl_log_memory) return;
    if (memory_to_log.find(lsaddr) != memory_to_log.end() ) 
    {
        cout << "Logging Mem Read for 0x"<< std::hex << lsaddr << " At PC:" << pc <<"\n";
    }

    if (labels[current_function].isRenamed || labels[current_function].doNotLog) {
        // only do it for new functions
        return;
    }

    if (labels[current_function].read_addresses.size() > NUMBER_OF_MANY_READS) {
        labels[current_function].many_memory_reads = true;
        return;
    }

    // auto currentMap = addresses[lsaddr];
    // currentMap[n2hexstr(lsaddr)]=labels[current_function].func_name+"("+n2hexstr(current_function)+"+"+n2hexstr(pc-current_function)+")";

    auto offset = pc-current_function;
    labels[current_function].read_addresses[n2hexstr(lsaddr)] = "func+0x"+n2hexstr(offset)+" pc=0x"+n2hexstr(pc);

    

}

void cdl_log_mem_write(const uint32_t lsaddr, uint32_t pc) {
    if (!cdl_log_memory) return;
    
    if (memory_to_log.find(lsaddr) != memory_to_log.end() ) 
    {
        cout << "Logging Mem Write to 0x"<< std::hex << lsaddr << " At PC:" << pc <<"\n";
    }

    if (labels[current_function].isRenamed || labels[current_function].doNotLog) {
        // only do it for new functions
        return;
    }

    if (labels[current_function].write_addresses.size() > NUMBER_OF_MANY_WRITES) {
        labels[current_function].many_memory_writes = true;
        return;
    }
    // auto currentMap = addresses[lsaddr];
    // currentMap[n2hexstr(lsaddr)]=labels[current_function].func_name+"("+n2hexstr(current_function)+"+"+n2hexstr(pc-current_function)+")";

    auto offset = pc-current_function;
    labels[current_function].write_addresses[n2hexstr(lsaddr)] = "+0x"+n2hexstr(offset)+" pc=0x"+n2hexstr(pc);

    
}

void cdl_hit_memory_log_point(uint32_t address) {
    if (address>0x10000000 && address <= 0x107fffff) {
        cout << "Cart Memory access!" << std::hex << address << " in:" << labels[current_function].func_name << "\n";
    }
}

void cdl_log_masked_write(uint32_t* address, uint32_t dst2) {
    if (!cdl_log_memory) return;
    // cout << "masked write:"<<std::hex<<dst<<" : "<<dst2<<"\n";
    // if (memory_to_log.find(address) != memory_to_log.end() ) 
    // {
    //     cout << "Logging Mem Write to 0x"<< std::hex << address << " At PC:" <<"\n";
    // }
}

void cdl_log_get_mem_handler(uint32_t address) {
    if (!cdl_log_memory) return;
    cdl_hit_memory_log_point(address);
    if (memory_to_log.find(address) != memory_to_log.end() ) 
    {
        cout << "Logging Mem cdl_log_get_mem_handler access 0x"<< std::hex << address <<"\n";
        cdl_hit_memory_log_point(address);
    }
}
void cdl_log_mem_read32(uint32_t address) {
    if (!cdl_log_memory) return;
    cdl_hit_memory_log_point(address);
    if (memory_to_log.find(address) != memory_to_log.end() ) 
    {
        cout << "Logging Mem cdl_log_mem_read32 access 0x"<< std::hex << address <<"\n";
        cdl_hit_memory_log_point(address);
    }
}
void cdl_log_mem_write32(uint32_t address) {
    if (!cdl_log_memory) return;
    cdl_hit_memory_log_point(address);
    if (memory_to_log.find(address) != memory_to_log.end() ) 
    {
        cout << "Logging Mem cdl_log_mem_write32 access 0x"<< std::hex << address <<"\n";
        cdl_hit_memory_log_point(address);
    }
}

string mapping_names[] = {
    "M64P_MEM_NOTHING",
    "M64P_MEM_NOTHING",
    "M64P_MEM_RDRAM",
    "M64P_MEM_RDRAMREG",
    "M64P_MEM_RSPMEM",
    "M64P_MEM_RSPREG",
    "M64P_MEM_RSP",
    "M64P_MEM_DP",
    "M64P_MEM_DPS",
    "M64P_MEM_VI",
    "M64P_MEM_AI",
    "M64P_MEM_PI",
    "M64P_MEM_RI",
    "M64P_MEM_SI",
    "M64P_MEM_FLASHRAMSTAT",
    "M64P_MEM_ROM",
    "M64P_MEM_PIF",
    "M64P_MEM_MI"
};

#define OSTASK_GFX 1
#define OSTASK_AUDIO 2

void cdl_log_ostask(uint32_t type, uint32_t flags, uint32_t bootcode, uint32_t bootSize, uint32_t ucode, uint32_t ucodeSize, uint32_t ucodeData, uint32_t ucodeDataSize) {
    if (!log_ostasks) return;
    if (rspboot == 0) {
        rspboot = map_assembly_offset_to_rom_offset(bootcode,0);
        auto bootDma = cdl_dma();
        bootDma.dram_start=rspboot;
        bootDma.dram_end = rspboot+bootSize;
        bootDma.rom_start = rspboot;
        bootDma.rom_end = rspboot+bootSize;
        bootDma.length = bootSize;
        bootDma.frame = l_CurFrame;
        bootDma.func_addr = print_function_stack_trace(); 
        bootDma.known_name = "rsp.boot";
        dmas[rspboot] = bootDma;
    }
    uint32_t ucodeRom = map_assembly_offset_to_rom_offset(ucode,0);

    if (dmas.find(ucodeRom) != dmas.end() ) 
        return;
    printf("OSTask type:%#08x flags:%#08x bootcode:%#08x ucode:%#08x ucodeSize:%#08x ucodeData:%#08x ucodeDataSize:%#08x \n", type, flags, bootcode, ucode, ucodeSize, ucodeData, ucodeDataSize);
    uint32_t ucodeDataRom = map_assembly_offset_to_rom_offset(ucodeData,0);

    auto data = cdl_dma();
    data.dram_start=ucodeData;
    data.dram_end = ucodeData+ucodeDataSize;
    data.rom_start = ucodeDataRom;
    data.rom_end = ucodeDataRom+ucodeDataSize;
    data.length = ucodeDataSize;
    data.frame = l_CurFrame;
    data.func_addr = print_function_stack_trace(); 
    data.is_assembly = false;

    auto t = cdl_dma();
    t.dram_start=ucode;
    t.dram_end = ucode+ucodeSize;
    t.rom_start = ucodeRom;
    t.rom_end = ucodeRom+ucodeSize;
    t.length = ucodeSize;
    t.frame = l_CurFrame;
    t.func_addr = print_function_stack_trace(); 
    t.is_assembly = false;


    if (type == OSTASK_AUDIO) {
        t.guess_type = "rsp.audio";
        t.ascii_header = "rsp.audio";
        t.known_name = "rsp.audio";
        data.ascii_header = "rsp.audio.data";
        data.known_name = "rsp.audio.data";
    } else if (type == OSTASK_GFX) {
        t.guess_type = "rsp.graphics";
        t.ascii_header = "rsp.graphics";
        t.known_name = "rsp.graphics";
        data.ascii_header = "rsp.graphics.data";
        data.known_name = "rsp.graphics.data";
    } else {
        printf("other type:%#08x  ucode:%#08x \n",type, ucodeRom);
    }
    dmas[ucodeRom] = t;
    dmas[ucodeDataRom] = data;
}


#define CDL_ALIST 0
#define CDL_UCODE_CRC 2
void cdl_log_rsp(uint32_t log_type, uint32_t address, const char * extra_data) {
    if (!log_rsp) return;
    if (log_type == CDL_ALIST) {
        if (audio_address.find(address) != audio_address.end() ) 
            return;
        audio_address[address] = n2hexstr(address)+extra_data;
        // cout << "Alist address:" << std::hex << address << " " << extra_data << "\n";
        return;
    }
    if (log_type == CDL_UCODE_CRC) {
        ucode_crc = n2hexstr(address);
        return;
    }
    cout << "Log rsp\n";
}

void cdl_log_dpc_reg_write(uint32_t address, uint32_t value, uint32_t mask) {
    cdl_common_log_tag("writeDPCRegs");
}


} // end extern C

// C++

uint32_t libRR_offset_to_look_for = 0x8149;
bool libRR_enable_look = false;

json libRR_disassembly = {};
json libRR_memory_reads = {};
json libRR_consecutive_rom_reads = {};
json libRR_called_functions = {};
json libRR_long_jumps = {};
int32_t previous_consecutive_rom_read = 0; // previous read address to check if this read is part of the chain
int16_t previous_consecutive_rom_bank = 0; // previous read address to check if this read is part of the chain
int32_t current_consecutive_rom_start = 0; // start address of the current chain

bool replace(std::string& str, const std::string from, const std::string to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

extern "C" void libRR_log_dma(int32_t offset) {
    if (offset> 0x7fff) {
        return;
    }
    cout << "DMA: " << n2hexstr(offset) << "\n";

}

static string label_name = "";
extern "C" const char* libRR_log_jump_label_with_name(uint32_t offset, uint32_t current_pc, const char* label_name) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return "";
    }
    
    string offset_str = n2hexstr(offset);
    int bank = get_current_bank_number_for_address(offset);
    string current_bank_str = n2hexstr(bank, 4);
    
    if (offset >libRR_slot_2_max_addr) {
        // if its greater than the max bank value then its probably in ram
        return "";
    }

    // debugging code start
    if (libRR_enable_look && (offset == libRR_offset_to_look_for || current_pc == libRR_offset_to_look_for)) {
        printf("Found long jump label with name offset: %d\n", libRR_offset_to_look_for);
    }
    // debugging code end

    // string label_name = "LAB_" + current_bank_str + "_" + n2hexstr(offset);
    if (!libRR_disassembly[current_bank_str][offset_str].contains("label_name")) {
        libRR_disassembly[current_bank_str][offset_str]["label_name"] = label_name;
    }
    libRR_disassembly[current_bank_str][offset_str]["meta"]["label_callers"][current_bank_str + "_" + n2hexstr(current_pc)] = true;
    return label_name;
}


extern "C" const char* libRR_log_jump_label(uint32_t offset, uint32_t current_pc) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return "_not_logging";
    }
    
    string offset_str = n2hexstr(offset);
    int bank = get_current_bank_number_for_address(offset);
    string current_bank_str = n2hexstr(bank, 4);
    
    if (offset >libRR_slot_2_max_addr) {
        // if its greater than the max bank value then its probably in ram
        return "max_bank_value";
    }

    // debugging code start
    if (libRR_enable_look && (offset == libRR_offset_to_look_for || current_pc == libRR_offset_to_look_for)) {
        printf("Found long jump label offset: %d\n", libRR_offset_to_look_for);
    }
    // debugging code end

    label_name = "LAB_" + current_bank_str + "_" + n2hexstr(offset);
    // return libRR_log_jump_label_with_name(offset, current_pc, label_name.c_str());

    if (!libRR_disassembly[current_bank_str][offset_str].contains("label_name")) {
        libRR_disassembly[current_bank_str][offset_str]["label_name"] = label_name;
    }
    libRR_disassembly[current_bank_str][offset_str]["meta"]["label_callers"][current_bank_str + "_" + n2hexstr(current_pc)] = true;
    return label_name.c_str();
}

extern "C" void libRR_log_memory_read(int8_t bank, int32_t offset, const char* type, uint8_t byte_size, char* bytes) {
    libRR_log_rom_read(bank, offset, type, byte_size, bytes);
}

extern "C" void libRR_log_rom_read(int16_t bank, int32_t offset, const char* type, uint8_t byte_size, char* bytes) {
    string bank_str = n2hexstr(bank, 4);
    string previous_bank_str = n2hexstr(previous_consecutive_rom_bank, 4);
    string offset_str = n2hexstr(offset);
    string current_consecutive_rom_start_str = n2hexstr(current_consecutive_rom_start);
    if (libRR_full_trace_log) {
        libRR_log_trace_str("Rom Read bank:"+bank_str+":"+n2hexstr(offset)+" = "+n2hexstr(bytes[0], 2));
    }
    // Check to see if the last read address is the same or 1 away
    // Check for the same is because sometimes data is checked by reading the first byte
    if (previous_consecutive_rom_bank == bank && previous_consecutive_rom_read == offset) {
        // do nothing if its the same byte read twice
        previous_consecutive_rom_bank = bank;
        libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str]["length"] = 1;
        return;
    }
    if (previous_consecutive_rom_bank == bank && previous_consecutive_rom_read == (offset-1)) {
        if (libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str].is_null()) {
            // check to see if the current read is null and if so create it
            libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str]["length"] = 1+ byte_size;
        } else {
            libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str]["length"] = ((uint32_t) libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str]["length"]) +byte_size;
        }
        for (int i=0; i<byte_size; i++) {
            libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str]["value"][n2hexstr(offset+i)] = n2hexstr(bytes[i]);
        }
    } 
    else {
        // cout << "previous consecutive length from:" << (int)previous_consecutive_rom_bank << "::" << n2hexstr(current_consecutive_rom_start) << " -> " << n2hexstr(previous_consecutive_rom_read) <<  " len:" << libRR_consecutive_rom_reads[previous_bank_str][current_consecutive_rom_start_str]["length"] << "\n";
        current_consecutive_rom_start = offset;
        current_consecutive_rom_start_str = n2hexstr(current_consecutive_rom_start);
        // initialise new consecutive run
        libRR_consecutive_rom_reads[bank_str][current_consecutive_rom_start_str]["length"] = 1;
        for (int i=0; i<byte_size; i++) {
            libRR_consecutive_rom_reads[bank_str][current_consecutive_rom_start_str]["value"][n2hexstr(offset+i)] = n2hexstr(bytes[i]);
        }
    }
    previous_consecutive_rom_read = offset+(byte_size-1); // add byte_size to take into account 2 byte reads
    previous_consecutive_rom_bank = bank;

    string value_str = "";
    if (byte_size == 2) {
        value_str = n2hexstr(two_bytes_to_16bit_value(bytes[1], bytes[0]));
    } else {
         value_str = n2hexstr(bytes[0]);
    }
    // printf("Access data: %d::%s type: %s size: %d value: %s\n", bank, n2hexstr(offset).c_str(), type, byte_size, value_str.c_str());
}

extern "C" void libRR_log_instruction_2int(uint32_t current_pc, const char* c_name, uint32_t instruction_bytes, int number_of_bytes, uint32_t operand, uint32_t operand2) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    std::string name(c_name);
    replace(name, "%int%", libRR_constant_replace(operand));
    replace(name, "%int2%", libRR_constant_replace(operand2));
    
    libRR_log_instruction(current_pc, name, instruction_bytes, number_of_bytes);
}
// Takes a single int argument and replaces it in the string
extern "C" void libRR_log_instruction_1int(uint32_t current_pc, const char* c_name, uint32_t instruction_bytes, int number_of_bytes, uint32_t operand) {
    return libRR_log_instruction_2int(current_pc, c_name, instruction_bytes, number_of_bytes, operand, 0);
}

extern "C" void libRR_log_instruction_1string(uint32_t current_pc, const char* c_name, uint32_t instruction_bytes, int number_of_bytes, const char* c_register_name) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    std::string name(c_name);
    std::string register_name(c_register_name);
    replace(name, "%str%",register_name);
    libRR_log_instruction_1int(current_pc, name.c_str(), instruction_bytes, number_of_bytes, 0x00);
}
extern "C" void libRR_log_instruction_1int_registername(uint32_t current_pc, const char* c_name, uint32_t instruction_bytes, int number_of_bytes, uint32_t operand, const char* c_register_name) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    std::string name(c_name);
    std::string register_name(c_register_name);
    replace(name, "%r%",register_name);
    libRR_log_instruction_1int(current_pc, name.c_str(), instruction_bytes, number_of_bytes, operand);
}

extern "C" void libRR_log_instruction_z80_s_d(uint32_t current_pc, const char* c_name, uint32_t instruction_bytes, int number_of_bytes, const char* source, const char* destination) {
     if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    std::string name(c_name);
    replace(name, "%s%", source);
    replace(name, "%d%", destination);
    
    libRR_log_instruction(current_pc, name, instruction_bytes, number_of_bytes);
}


// 
// Z80 End
// 

// current_pc - current program counter
// instruction bytes as integer used for hex
// arguments  - number of arguments - currently not really used for anything
// m - used for register number, replaces Rm with R1/R2 etc
void libRR_log_instruction(uint32_t current_pc, string name, uint32_t instruction_bytes, int number_of_bytes, unsigned m, unsigned n, unsigned imm, unsigned d, unsigned ea) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    replace(name, "%EA", "0x"+n2hexstr(ea));
    libRR_log_instruction(current_pc, name, instruction_bytes, number_of_bytes, m, n, imm, d);
}
void libRR_log_instruction(uint32_t current_pc, string name, uint32_t instruction_bytes, int number_of_bytes, unsigned m, unsigned n, unsigned imm, unsigned d) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    replace(name, "#imm", "#"+to_string(imm));
    replace(name, "disp", ""+to_string(d));
    if (name.find("SysRegs") != std::string::npos) {
        replace(name, "SysRegs[#0]", "MACH");
        replace(name, "SysRegs[#1]", "MACL");
        replace(name, "SysRegs[#2]", "PR");
    }
    libRR_log_instruction(current_pc, name, instruction_bytes, number_of_bytes, m, n);
}


void libRR_log_instruction(uint32_t current_pc, string name, uint32_t instruction_bytes, int number_of_bytes, unsigned m, unsigned n) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    replace(name, "Rm", "R"+to_string(m));
    replace(name, "Rn", "R"+to_string(n));
    libRR_log_instruction(current_pc, name, instruction_bytes, number_of_bytes);
}

extern "C" void libRR_log_instruction(uint32_t current_pc, const char* name, uint32_t instruction_bytes, int number_of_bytes)
{
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }
    // printf("libRR_log_instruction pc:%d name: %s bytes: %d\n", current_pc, name, instruction_bytes);
    std::string str(name);
    libRR_log_instruction(current_pc, str, instruction_bytes, number_of_bytes);
}

// C version of the c++ template
extern "C" const char* n2hexstr_c(int number, size_t hex_len) {
    return n2hexstr(number, hex_len).c_str();
}

string libRR_constant_replace(uint32_t da8) {
    string addr_str = n2hexstr(da8);
    if (libRR_console_constants["addresses"].contains(addr_str)) {
        return libRR_console_constants["addresses"][addr_str];
    }
    return "$"+n2hexstr(da8);
}

int32_t previous_pc = 0; // used for debugging
bool has_read_first_ever_instruction = false;
void libRR_log_instruction(uint32_t current_pc, string name, uint32_t instruction_bytes, int number_of_bytes) {
    if (!libRR_full_function_log || !libRR_finished_boot_rom) {
        return;
    }

    if (!has_read_first_ever_instruction) {
        // special handling for the entry point, we wanr to force a label here to it gets written to output
        libRR_log_jump_label_with_name(current_pc, current_pc, "entry");
        has_read_first_ever_instruction = true;
        libRR_isDelaySlot = false;
    }

    int bank = get_current_bank_number_for_address(current_pc);
    string current_bank_str = n2hexstr(bank, 4);

    // trace log each instruction
    if (libRR_full_trace_log) {
        libRR_log_trace_str(name + "; pc:"+current_bank_str+":"+n2hexstr(current_pc));
    }

    // Code used for debugging why an address was reached
    if (libRR_enable_look && current_pc == libRR_offset_to_look_for) {
        printf("Reached %s: previous addr: %s name:%s bank:%d \n ", n2hexstr(libRR_offset_to_look_for).c_str(), n2hexstr(previous_pc).c_str(), name.c_str(), bank);
    }
    // end debugging code
    
    if (strcmp(libRR_console,"Saturn")==0) {
        printf("isSaturn\n");
        // For saturn we remove 2 from the program counter, but this will vary per console
        current_pc -= 4; // was -2
    }


    // string current_function = n2hexstr(function_stack.back());
    string current_pc_str = n2hexstr(current_pc);
    // printf("libRR_log_instruction %s \n", current_function.c_str());
    if (strcmp(libRR_console,"Saturn")==0) {
        if (libRR_isDelaySlot) {
            current_pc_str = n2hexstr(libRR_delay_slot_pc - 2); //subtract 2 as pc is ahead
            // printf("Delay Slot %s \n", current_pc_str.c_str());
            libRR_isDelaySlot = false;
        }
    }

    // TODO: Hex bytes should change based on number_of_bytes
    string hexBytes = n2hexstr((uint32_t)instruction_bytes, number_of_bytes*2);
    

    // if we are below the max addr of bank 0 (e.g 0x4000 for GB) then we are always in bank 0
    // if (current_pc <libRR_slot_0_max_addr) {
    //     current_bank_str="0000";
    // }

    // libRR_disassembly[current_bank_str][current_pc_str][name]["frame"]=RRCurrentFrame;
    libRR_disassembly[current_bank_str][current_pc_str][name]["bytes"]=hexBytes;
    libRR_disassembly[current_bank_str][current_pc_str][name]["bytes_length"]=number_of_bytes;
    previous_pc = current_pc;
}