#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <mutex>
#include <chrono>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <bits/stdc++.h>
#include <list>
#include <map>
#include <cstdint>

// Forward declarations
class Process;
class Scheduler;
class FCFSScheduler;
class RoundRobinScheduler;
bool accessMemory(Process* proc, int vpn, int curCycle);
void handlePageFault(Process* proc, int vpn, unsigned long long curCycle);


// Global variables
//std::vector<std::unique_ptr<Process>> allProcesses;
std::list<std::unique_ptr<Process>> allProcesses;
std::vector<Process*> runningProcesses;
std::vector<Process*> finishedProcesses;
Scheduler* scheduler = nullptr;
std::mutex processMutex;

//new additon, remove when seen
std::mutex memoryMutex;

std::atomic<bool> schedulerRunning{false};
std::atomic<int> processIdCounter{1};
std::atomic<long long> global_simulated_cycles{0}; // Global variable to track simulated cycles
std::atomic<bool> processCreationRunning{false}; 
std::atomic<int> autoProcessCounter{0}; //for tracking auto-generated screen names

//NEW
//std::vector<bool> memoryBlock; //true = used, false = free
std::map<int, int> memoryBlock; //vector to map

std::mutex memoryAccessMutex;
std::map<uint16_t, int> memoryAccessTable; // addr -> process ID 

//FOR CONFIG.txt
int numCPU; //number of cores (between 1-128 inclusive)
std::string schedulerType; //scheduler type("fcfs" or "rr")
uint64_t quantumCycles; //for round robin, how many ticks before swapping (1-2^32 inclusive)
uint64_t batchProcessFreq; //1 process every x cycles (1-2^32 inclusive)
uint64_t minIns;
uint64_t maxIns;
uint64_t delaysPerExec; //1 instruction every x cycles (0 - 2^32 inclusive) if 0, it executes every cycle
uint16_t maxOverallMem;
uint16_t memPerFrame;
uint16_t memPerProc;
uint16_t minMemPerProc; //minimum memory per process
uint16_t maxMemPerProc; //maximum memory per process
uint16_t totalFrames = 0;
bool initialized = false;

//Instruction Types
enum class InstructionType {
    PRINT,
    ADD,
    DECLARE,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    SLEEP,
    FOR,
    READ,
    WRITE
};

struct Instruction {
    InstructionType type;
    std::vector<std::string> args;
    std::vector<Instruction> nestedInstructions;
};

//for screen -c
std::vector<Instruction> parseUserInstructions(const std::string& input) {
    std::vector<Instruction> result;
    std::istringstream stream(input);
    std::string token;

    std::cout << "[DEBUG] Full input string: [" << input << "]\n";

    // Split by semicolon
    while (std::getline(stream, token, ';')) {
        std::cout << "[DEBUG] Processing token: [" << token << "]\n";
        
        // Trim whitespace from token
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);
        
        if (token.empty()) {
            std::cout << "[DEBUG] Empty token, skipping\n";
            continue;
        }
        
        std::string op;
        size_t op_end = token.find_first_of(" (");
        if (op_end != std::string::npos) {
            op = token.substr(0, op_end);
        } else {
            op = token;
        }
        
        Instruction instr;
        instr.args.clear();
        std::cout << "[DEBUG] Operation: [" << op << "]\n";
        
        if (op == "PRINT") {
            instr.type = InstructionType::PRINT;
            
            // Find the parentheses in the token (not just instrStream)
            size_t start = token.find('(');
            size_t end = token.rfind(')');
            
            std::cout << "[DEBUG] PRINT token: [" << token << "]\n";
            std::cout << "[DEBUG] Parentheses at: " << start << " and " << end << "\n";

            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string inside = token.substr(start + 1, end - start - 1);
                std::cout << "[DEBUG] Inside parentheses: [" << inside << "]\n";

                // Unescape \" → "
                std::string cleaned;
                bool escape = false;
                for (char ch : inside) {
                    if (escape) {
                        if (ch == '\"') cleaned += '\"';
                        else {
                            cleaned += '\\';
                            cleaned += ch;
                        }
                        escape = false;
                    } else if (ch == '\\') {
                        escape = true;
                    } else {
                        cleaned += ch;
                    }
                }
                if (escape) cleaned += '\\';

                // Trim leading/trailing whitespace
                cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
                cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);

                instr.args.push_back(cleaned);
                std::cout << "[DEBUG] Parsed PRINT argument: [" << cleaned << "]\n";
            } else {
                std::cerr << "Warning: Malformed PRINT instruction: " << token << std::endl;
                continue;
            }
        }
        else if (op == "DECLARE") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string var, value;
            instrStream >> var >> value;
            if (var.empty() || value.empty()) {
                std::cerr << "Warning: Invalid DECLARE instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::DECLARE;
            instr.args = {var, value};
            std::cout << "[DEBUG] Parsed DECLARE: " << var << " = " << value << "\n";
        } 
        else if (op == "ADD") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty()) {
                std::cerr << "Warning: Invalid ADD instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::ADD;
            instr.args = {a, b, c};
            std::cout << "[DEBUG] Parsed ADD: " << a << " = " << b << " + " << c << "\n";
        } 
        else if (op == "SUBTRACT") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty()) {
                std::cerr << "Warning: Invalid SUBTRACT instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::SUBTRACT;
            instr.args = {a, b, c};
        } 
        else if (op == "WRITE") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string addr, var;
            instrStream >> addr >> var;
            if (addr.empty() || var.empty()) {
                std::cerr << "Warning: Invalid WRITE instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::WRITE;
            instr.args = {addr, var};
        } 
        else if (op == "READ") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string var, addr;
            instrStream >> var >> addr;
            if (var.empty() || addr.empty()) {
                std::cerr << "Warning: Invalid READ instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::READ;
            instr.args = {var, addr};
        } 
        else if (op == "SLEEP") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string ticks;
            instrStream >> ticks;
            if (ticks.empty()) {
                std::cerr << "Warning: Invalid SLEEP instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::SLEEP;
            instr.args = {ticks};
        } 
        else if (op == "MULTIPLY") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty()) {
                std::cerr << "Warning: Invalid MULTIPLY instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::MULTIPLY;
            instr.args = {a, b, c};
        } 
        else if (op == "DIVIDE") {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty()) {
                std::cerr << "Warning: Invalid DIVIDE instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::DIVIDE;
            instr.args = {a, b, c};
        } 
        else {
            std::cerr << "Warning: Unknown instruction type: " << op << std::endl;
            continue;
        }
        
        std::cout << "[DEBUG] Adding instruction: " << op << std::endl;
        result.push_back(instr);
    }

    std::cout << "[DEBUG] Total instructions parsed: " << result.size() << std::endl;
    return result;
}

//NEW
// struct FrameEntry{
//     int processId = -1;
//     int virtualPage = -1;
//     uint64_t lastUsed = 0;
//     bool occupied = false;
// };

// std::deque<int> freeFrames;
// std::vector<FrameEntry> frameTable(totalFrames);

// std::atomic<uint64_t> numPagedIn{0};
// std::atomic<uint64_t> numPagedOut{0};
//END OF NEW
//NEW
bool isValidMemorySize(uint16_t num){
    return (num >= 64) && (num <= 65536) && ((num & (num - 1)) == 0);
}

uint16_t getMemorySize(){
    int minExponent = static_cast<int>(std::log2(minMemPerProc));
    int maxExponent = static_cast<int>(std::log2(maxMemPerProc));
    int range = maxExponent - minExponent + 1;
    int randomExponent = minExponent + (std::rand() % range);
    return static_cast<uint16_t>(1 << randomExponent);
}

bool readConfig(){
    std::ifstream file("config.txt");
    std::string line;
    bool outOfRangeCPU = false;
    bool outOfRangeScheduler = false;
    bool outOfRangeQuantum = false;
    bool outOfRangeBatch = false;
    bool outOfRangeMin = false;
    bool outOfRangeMax = false;
    bool outOfRangeDelay = false;
    bool outOfRangeMaxMem = false;
    bool outOfRangeMemPerFrame = false;
    bool outOfRangeMinMemPerProc = false;
    bool outOfRangeMaxMemPerProc = false;

    while(std::getline(file, line)){
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        

        if (key == "num-cpu") {
            iss >> numCPU;
            if (numCPU < 1 || numCPU > 128){
                outOfRangeCPU = true;
                std::cout << "Error: num-cpu must be between 1 and 128 (inclusive). Please reconfigure config.txt." << std::endl;
            }
                 
        } 
        else if (key == "scheduler") {
            iss >> std::quoted(schedulerType);
            if(schedulerType != "fcfs" && schedulerType != "rr"){
                outOfRangeScheduler = true;
                std::cout << "Error: scheduler can only be fcfs or rr. Please reconfigure config.txt." << std::endl;
            }
                
        } 
        else if (key == "quantum-cycles") {
            iss >> quantumCycles;
            if (quantumCycles < 0 || quantumCycles > 4294967296){
                outOfRangeQuantum = true;
                std::cout << "Error: quantum-cycles must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
                
        } 
        else if (key == "batch-process-freq") {
            iss >> batchProcessFreq;
            if (batchProcessFreq < 1 || batchProcessFreq > 4294967296){
                outOfRangeBatch = true;
                std::cout << "Error: batch-process-freq must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
                
        }
        else if (key == "min-ins") {
            iss >> minIns;
            if (minIns < 1 || minIns > 4294967296){
                outOfRangeMin = true;
                std::cout << "Error: min-ins must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        } 
        else if (key == "max-ins") {
            iss >> maxIns;
            if (maxIns < 1 || maxIns > 4294967296){
                outOfRangeMax = true;
                std::cout << "Error: max-ins must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
            if (maxIns < minIns){
                outOfRangeMax = true;
                std::cout << "Error: max-ins must be greater than or equal to min-ins. Please reconfigure config.txt." << std::endl;

            }
        } 
        else if (key == "delays-per-exec") {
            iss >> delaysPerExec;
            if (delaysPerExec < 0 || delaysPerExec > 4294967296){
                outOfRangeDelay = true;
                std::cout << "Error: delays-per-exec must be between 0 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "max-overall-mem"){
            iss >> maxOverallMem;
            if (maxOverallMem < 1 || maxOverallMem > 4294967296){
                outOfRangeMaxMem = true;
                std::cout << "Error: max-overall-mem must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "mem-per-frame"){
            iss >> memPerFrame;
            if (memPerFrame < 1 || memPerFrame > 4294967296){
                outOfRangeMemPerFrame = true;
                std::cout << "Error: mem-per-frame must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "min-mem-per-proc"){
            iss >> minMemPerProc;
            if (((isValidMemorySize(minMemPerProc)) == false)){
                outOfRangeMinMemPerProc = true;
                std::cout << "Error: min-mem-per-proc must be a power of two between 64 and 65536 (inclusive). Please reconfigure config.txt." << std::endl;
            }
            if (minMemPerProc > maxOverallMem){
                outOfRangeMinMemPerProc = true;
                std::cout << "Error: min-mem-per-proc must be less than or equal to max-overall-mem. Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "max-mem-per-proc"){
            iss >> maxMemPerProc;
            if ((isValidMemorySize(maxMemPerProc)) == false){
                outOfRangeMaxMem = true;
                std::cout << "Error: max-mem-per-proc must be a power of two between 64 and 65536 (inclusive). Please reconfigure config.txt." << std::endl;
            }
            if (maxMemPerProc < minMemPerProc){
                outOfRangeMaxMemPerProc = true;
                std::cout << "Error: max-mem-per-proc must be greater than or equal to min-mem-per-proc. Please reconfigure config.txt." << std::endl;
            }
            if (maxMemPerProc > maxOverallMem){
                outOfRangeMaxMemPerProc = true;
                std::cout << "Error: max-mem-per-proc must be less than or equal to max-overall-mem. Please reconfigure config.txt." << std::endl;
            }
        }

    }
    if (outOfRangeCPU || outOfRangeScheduler || outOfRangeQuantum || outOfRangeBatch || outOfRangeMin || outOfRangeMax || outOfRangeDelay || outOfRangeMaxMem || outOfRangeMemPerFrame || outOfRangeMinMemPerProc || outOfRangeMaxMemPerProc)
        return false;
    totalFrames = maxOverallMem / memPerFrame;
    //memoryBlock = std::vector<bool>(totalFrames, false); // Initialize memory usage tracking
    return true;
}
//NEW
std::vector<int> readPageFromBackingStore(int pid, int vpn) {
    std::ifstream in("backing_store.txt");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        int filePid, fileVpn;
        iss >> filePid >> fileVpn;
        if (filePid == pid && fileVpn == vpn) {
            std::vector<int> data;
            int val;
            while (iss >> val) data.push_back(val);
            return data;
        }
    }
    return std::vector<int>(8, 0);  // default page
}

void writePageToBackingStore(int pid, int vpn, const std::vector<int>& data) {
    std::ofstream out("backing_store.txt", std::ios::app);
    out << pid << " " << vpn;
    for (int val : data) out << " " << val;
    out << "\n";
}
//END OF NEW
//NEW
int allocateMemory(int framesNeeded) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    if (memoryBlock.empty()) {
        if (framesNeeded <= totalFrames) {
            memoryBlock[0] = framesNeeded;
            return 0;
        } else {
            return -1;
        }
    }

    int lastEnd = 0;
    for (const auto& [start, size] : memoryBlock) {
        int gap = start - lastEnd;
        if (gap >= framesNeeded) {
            memoryBlock[lastEnd] = framesNeeded;
            return lastEnd;
        }
        lastEnd = start + size;
    }

    // Check space at the end
    if (totalFrames - lastEnd >= framesNeeded) {
        memoryBlock[lastEnd] = framesNeeded;
        return lastEnd;
    }

    return -1; // Not enough space
}

//NEW
void freeMemory(int startIndex, int frames) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    auto it = memoryBlock.find(startIndex);
    if (it != memoryBlock.end() && it->second == frames) {
        memoryBlock.erase(it);
    }
}

//NEW
int countExternalFragmentation() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    int freeFrames = 0;
    int lastEnd = 0;

    for (const auto& [start, size] : memoryBlock) {
        freeFrames += (start - lastEnd); // space between blocks
        lastEnd = start + size;
    }

    // Space at the end
    freeFrames += (totalFrames - lastEnd);

    return freeFrames * memPerFrame; // in KB
}


// Process class
class Process {
private:
    std::string name; // name of the process based from user input
    int id; // process id 
    int totalInstructions; // total number of instructions that the process needs to run
    int remainingInstructions; // number of instructions in the process queue
    int currentInstruction; // line of instruction the process is currently in
    std::string timeCreated; // time the process was created, includes date and time
    int assignedCore; // the process's assigned core to execute in
    std::unique_ptr<std::ofstream> logFile; // stores the logfile of past processes executed
    std::mutex processExecutionMutex; // Mutex to protect process execution
    std::vector<std::string> logs; // Store execution logs
    std::atomic<int> cyclesSinceLastExec{0}; // For delays-per-exec implementation
    std::vector<Instruction> instructions;  // list of instructions the process will implement
    std::map<std::string, uint16_t> variables; // variables that will be declared during the process
    std::atomic<uint64_t> sleepUntilCycle{0}; // for applying sleep to a process
    std::vector<int> forLoopCounters; // Stack for nested for loops
    std::vector<int> forLoopMaxRepeats; 
    //NEW
    int memoryStartIndex = -1;
    int framesAllocated = 0;

    uint16_t requiredMemorySize;

public:

    void shutdownDueToMemoryViolation(uint16_t address) {
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        char timeBuf[9]; // HH:MM:SS
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localTime);

        std::ostringstream oss;
        oss << "Process " << name << " shut down due to memory access violation error that occurred at "
            << std::string(timeBuf) << ". 0x" << std::hex << std::uppercase << address << " invalid.";


        std::string errorMsg = oss.str();
        logs.push_back(errorMsg);

        std::cout << "[ERROR]" << errorMsg << std::endl;
        remainingInstructions = 0;
    }

    // Constructor
    // update with memory size
    Process(const std::string& processName, int numInstructions = 100, uint16_t memSize = 0) 
        : name(processName), totalInstructions(numInstructions), 
          remainingInstructions(numInstructions), currentInstruction(0), assignedCore(-1), requiredMemorySize(memSize) {
        
        id = processIdCounter++;
        
        // Set creation time
        std::time_t timestamp;
        std::time(&timestamp);
        char buffer[30];
        std::tm* timeinfo = std::localtime(&timestamp);
        std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
        timeCreated = buffer;

        generateRandomInstructions(numInstructions);
    }
    
    // Delete copy constructor and assignment operator to prevent copying
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    
    // Move constructor
    Process(Process&& other) noexcept
        : name(std::move(other.name)), id(other.id), totalInstructions(other.totalInstructions),
          remainingInstructions(other.remainingInstructions), currentInstruction(other.currentInstruction),
          timeCreated(std::move(other.timeCreated)), assignedCore(other.assignedCore),
          logFile(std::move(other.logFile)), instructions(std::move(other.instructions)),
          variables(std::move(other.variables)),
          cyclesSinceLastExec(other.cyclesSinceLastExec.load()),
          sleepUntilCycle(other.sleepUntilCycle.load()) {
    }
    
    // Move assignment operator
    Process& operator=(Process&& other) noexcept {
    if (this != &other) {
        name = std::move(other.name);
        id = other.id;
        totalInstructions = other.totalInstructions;
        remainingInstructions = other.remainingInstructions;
        currentInstruction = other.currentInstruction;
        timeCreated = std::move(other.timeCreated);
        assignedCore = other.assignedCore;
        logFile = std::move(other.logFile);
        instructions = std::move(other.instructions);
        variables = std::move(other.variables);
        cyclesSinceLastExec.store(other.cyclesSinceLastExec.load());
        sleepUntilCycle.store(other.sleepUntilCycle.load());
    }
    return *this;
    }
    
    // Destructor
    ~Process() {
        if (logFile && logFile->is_open()) {
            logFile->close();
        }
    }
    // struct PageTableEntry {
    //     int frameIndex = -1;
    //     bool present = false;
    //     bool dirty = false;
    //     uint64_t lastUsed = 0;
    // };

    // std::vector<PageTableEntry> pageTable;

    // void initPageTable(int numPages) {
    //     pageTable.resize(numPages);
    // }

    // PageTableEntry& getPage(int vpn) {
    //     return pageTable[vpn];
    // }

    //NEW
    int getMemoryStartIndex() const {
        return memoryStartIndex;
    }
    //NEW
    int getFramesAllocated() const {
        return framesAllocated;
    }
    //NEW
    void setMemoryAllocation(int startIndex, int frames) {
        memoryStartIndex = startIndex;
        framesAllocated = frames;
    }
    // for instructions not sureee
    void generateRandomInstructions(int numInstructions, int depth = 0) {
        instructions.clear();
        for (int i = 0; i < numInstructions; ++i) {
            Instruction instr;
            int instrType = std::rand() % 6;
            switch (instrType) {
                case 0: // PRINT
                    instr.type = InstructionType::PRINT;
                    instr.args.push_back("\"Hello world from " + name + "!\"");
                    break;
                case 1: // DECLARE
                    instr.type = InstructionType::DECLARE;
                    instr.args.push_back("var" + std::to_string(std::rand() % 10));
                    instr.args.push_back(std::to_string(std::rand() % 65536));
                    break;
                case 2: // ADD
                    instr.type = InstructionType::ADD;
                    instr.args.push_back("var" + std::to_string(std::rand() % 10));
                    instr.args.push_back("var" + std::to_string(std::rand() % 10));
                    instr.args.push_back(std::to_string(std::rand() % 65536));
                    break;
                case 3: // SUBTRACT
                    instr.type = InstructionType::SUBTRACT;
                    instr.args.push_back("var" + std::to_string(std::rand() % 10));
                    instr.args.push_back("var" + std::to_string(std::rand() % 10));
                    instr.args.push_back(std::to_string(std::rand() % 65536));
                    break;
                case 4: // SLEEP
                    instr.type = InstructionType::SLEEP;
                    instr.args.push_back(std::to_string(std::rand() % 255 + 1)); // Sleep for 1-255 ticks
                    break;
                case 5: // FOR
                    if (depth < 3) { // Nest up to 3 times
                        instr.type = InstructionType::FOR;
                        int repeats = std::rand() % 5 + 1;
                        instr.args.push_back(std::to_string(repeats));
                        int nestedInstructionCount = std::rand() % 3 + 1;
                        for(int j=0; j < nestedInstructionCount; ++j) {
                            // Simplified nested instruction generation
                            Instruction nested;
                            nested.type = InstructionType::PRINT;
                            nested.args.push_back("\"Nested loop says hi!\"");
                            instr.nestedInstructions.push_back(nested);
                        }
                    } else { // Fallback to PRINT if too deep
                        instr.type = InstructionType::PRINT;
                        instr.args.push_back("\"Max nesting reached!\"");
                    }
                    break;
            }
            instructions.push_back(instr);
        }
        totalInstructions = instructions.size();
        remainingInstructions = instructions.size();
    }
    
    // Execute one instruction of the process
    bool executeInstruction(int coreId) {
        std::lock_guard<std::mutex> lock(processExecutionMutex);

        if (global_simulated_cycles < sleepUntilCycle) {
            return true;
        }
        
        if (delaysPerExec > 0) {
            cyclesSinceLastExec++;
            if (cyclesSinceLastExec <= delaysPerExec) {
                return true;
            }
            cyclesSinceLastExec = 0;
        }
        
        if (currentInstruction >= instructions.size()) {
                return false;
            }

            const auto& instr = instructions[currentInstruction];
            assignedCore = coreId;
            std::ostringstream oss;

            switch (instr.type) {
                case InstructionType::PRINT: {
                    std::string msg = instr.args[0];
                    // Simple variable replacement
                    size_t pos = msg.find("+");
                    if (pos != std::string::npos) {
                        std::string varName = msg.substr(pos + 1);
                        // trim whitespace
                        varName.erase(std::remove_if(varName.begin(), varName.end(), ::isspace), varName.end());
                        msg = msg.substr(0, pos);
                        oss << msg.substr(1, msg.length() - 2) << (variables.count(varName) ? std::to_string(variables[varName]) : "0");
                    } else {
                        oss << msg.substr(1, msg.length() - 2);
                    }
                    break;
                }
                //symbol table (variables)
                case InstructionType::DECLARE: {
                    if (variables.size() >= 32){
                        oss << "Symbol table full." << instr.args[0];
                    } else {
                        variables[instr.args[0]] = static_cast<uint16_t>(std::stoul(instr.args[1]));
                        oss << "Declared " << instr.args[0] << " = " << instr.args[1];
                        // int virtualPage = rand() % pageTable.size();
                        // accessMemory(this, virtualPage, global_simulated_cycles);
                        // getPage(virtualPage).dirty = true;
                    }
                    break;
                }
                case InstructionType::ADD: {
                    uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
                    uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
                    variables[instr.args[0]] = val2 + val3;
                    oss << "ADD: " << instr.args[0] << " = " << val2 << " + " << val3 << " -> " << variables[instr.args[0]];
                    // int virtualPage = rand() % pageTable.size(); //NEW
                    // accessMemory(this, virtualPage, global_simulated_cycles);
                    // getPage(virtualPage).dirty = true; //END OF NEW
                    break;
                }
                case InstructionType::SUBTRACT: {
                    uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
                    uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
                    uint16_t result = (val2 > val3) ? val2 - val3 : 0; // Clamp at 0
                    variables[instr.args[0]] = result;
                    oss << "SUBTRACT: " << instr.args[0] << " = " << val2 << " - " << val3 << " -> " << result;
                    // int virtualPage = rand() % pageTable.size();
                    // accessMemory(this, virtualPage, global_simulated_cycles);
                    // getPage(virtualPage).dirty = true;
                    break;
                }
                case InstructionType::SLEEP: {
                    uint8_t sleepTicks = static_cast<uint8_t>(std::stoul(instr.args[0]));
                    sleepUntilCycle = global_simulated_cycles + sleepTicks;
                    oss << "Sleeping for " << std::to_string(sleepTicks) << " cycles.";
                    break;
                }
                case InstructionType::MULTIPLY: {
                    uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
                    uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
                    variables[instr.args[0]] = val2 * val3;
                    oss << "MULTIPLY: " << instr.args[0] << " = " << val2 << " * " << val3 << " -> " << variables[instr.args[0]];
                    // int virtualPage = rand() % pageTable.size();
                    // accessMemory(this, virtualPage, global_simulated_cycles);
                    // getPage(virtualPage).dirty = true;

                    break;
                }           
                case InstructionType::DIVIDE: {
                    uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
                    uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
                    uint16_t result = (val3 == 0) ? 0 : (val2 / val3);
                    variables[instr.args[0]] = result;
                    oss << "DIVIDE: " << instr.args[0] << " = " << val2 << " / " << val3 << " -> " << result;
                    // int virtualPage = rand() % pageTable.size();
                    // accessMemory(this, virtualPage, global_simulated_cycles);
                    // getPage(virtualPage).dirty = true;

                    break;
                }
                case InstructionType::FOR:
                    // This is a simplified placeholder. A full implementation would require more complex state management.
                    // For now, we just execute the nested instructions.
                    for (const auto& nested_instr : instr.nestedInstructions) {
                        // For simplicity, we just log a message for nested execution
                        logs.push_back("Executing nested instruction inside FOR loop.");
                    }
                    oss << "Executed a FOR loop.";
                    break;
                
                // READ FUNCTION
                case InstructionType::READ: {
                    const std::string& varName = instr.args[0];
                    const std::string& addrStr = instr.args[1];

                    uint16_t addr = 0;
                    try {
                        addr = std::stoi(addrStr, nullptr, 16);     // Parse hex
                    } catch (...) {
                    oss << "READ ERROR: Invalid address format: " << addrStr;
                    break;
                    }

                    // checks whether the input memory is in range or not
                    int memStart = getMemoryStartIndex() * memPerFrame;
                    int memEnd = memStart + (getFramesAllocated() * memPerFrame);

                    if (addr < memStart || addr + 1 >= memEnd) {
                        shutdownDueToMemoryViolation(addr);
                        return false;
                    }

                    // Lock access to shared memory
                    {
                        std::lock_guard<std::mutex> lock(memoryAccessMutex);
                        if (memoryAccessTable.count(addr) && memoryAccessTable[addr] != id) {
                            shutdownDueToMemoryViolation(addr);
                            return false;
                        }
                        memoryAccessTable[addr] = id;
                    }

                    // Proceed with the read
                    uint8_t low  = memoryBlock.count(addr)     ? memoryBlock[addr]     : 0;
                    uint8_t high = memoryBlock.count(addr + 1) ? memoryBlock[addr + 1] : 0;
                    uint16_t value = (high << 8) | low;
                    variables[varName] = value;

                    oss << "READ " << varName << " = mem[" << addrStr << "] -> " << value;

                    // Release access
                    {
                        std::lock_guard<std::mutex> lock(memoryAccessMutex);
                        memoryAccessTable.erase(addr);
                    }

                    // 0xFFFF is harcoded due to using dynamic memory map. this is a max bounded check
                    
                    /*if (addr > 0xFFFF - 1) {
                        oss << "READ ERROR: Address out of bounds.";
                        break;
                    }

                    uint8_t low  = memoryBlock.count(addr)     ? memoryBlock[addr]     : 0;
                    uint8_t high = memoryBlock.count(addr + 1) ? memoryBlock[addr + 1] : 0;

                    uint16_t value = (high << 8) | low;
                    variables[varName] = value;
                    //uint16_t value = (memory[addr + 1] << 8) | memory[addr];
                    //variables[varName] = value;

                    oss << "READ " << varName << " = mem[" << addrStr << "] -> " << value;
                    break;*/
                }
                
                // WRITE FUNCTION
                case InstructionType::WRITE: {
                    const std::string& addrStr = instr.args[0];
                    const std::string& valStr = instr.args[1];

                    uint16_t addr = 0;
                    uint16_t value = 0;
                    try {
                        addr = std::stoi(addrStr, nullptr, 16);         //Parse hex address
                        value = std::stoi(valStr);                      // parse value
                    } catch (...) {
                        oss << "WRITE ERROR: Invalid address or value.";
                        break;
                    }
                    
                    // checks whether the input memory is in range or not 
                    int memStart = getMemoryStartIndex() * memPerFrame;
                    int memEnd = memStart + (getFramesAllocated() * memPerFrame);

                    if (addr < memStart || addr + 1 >= memEnd) {
                        shutdownDueToMemoryViolation(addr);
                        return false;
                    }

                    // Lock access to shared memory
                    {
                        std::lock_guard<std::mutex> lock(memoryAccessMutex);
                        if (memoryAccessTable.count(addr) && memoryAccessTable[addr] != id) {
                            shutdownDueToMemoryViolation(addr);
                            return false;
                        }
                        memoryAccessTable[addr] = id;
                    }

                    // Perform the write
                    value = std::min<uint16_t>(value, std::numeric_limits<uint16_t>::max());
                    memoryBlock[addr]     = static_cast<uint8_t>(value & 0xFF);
                    memoryBlock[addr + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);

                    oss << "WRITE mem[" << addrStr << "] = " << value;

                    // Release access
                    {
                        std::lock_guard<std::mutex> lock(memoryAccessMutex);
                        memoryAccessTable.erase(addr);
                    }

                    // 0xFFFF is harcoded due to using dynamic memory map. this is a max bounded check
                    /*if (addr > 0xFFFF - 1) {
                        oss << "WRITE ERROR: Address out of bounds.";
                        break;
                    }

                    value = std::min<uint16_t>(value, std::numeric_limits<uint16_t>::max()); // clamp value to uint16_t
                    memoryBlock[addr]     = static_cast<uint8_t>(value & 0xFF);        // Low byte
                    memoryBlock[addr + 1] = static_cast<uint8_t>((value >> 8) & 0xFF); // High byte
                    //memory[addr] = value & 0xFF;
                    //memory[addr + 1] = (value >> 8) & 0xFF;

                    oss << "WRITE mem[" << addrStr << "] = " << value;
                    break;*/
                }

            }
            
            // Log the print command
            std::time_t timestamp; // Get current time
            std::time(&timestamp); // Get current time in seconds since epoch
            char buffer[30]; // Format time as MM/DD/YYYY HH:MM:SS AM/PM
            std::tm* timeinfo = std::localtime(&timestamp); // Convert to local time
            std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo); // Format time
           
           // Log the print command

            std::ostringstream log_entry;
            log_entry << "(" << buffer << ") Core:" << coreId << " " << oss.str();
            logs.push_back(log_entry.str());

            currentInstruction++;
            remainingInstructions = instructions.size() - currentInstruction;


            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            return true;
    }
    
    bool canExecute() const {
        return !hasFinished();
    }
    
    // Getters
    int getRemainingInstructions() const { return remainingInstructions; }
    int getCurrentInstruction() const { return currentInstruction; }
    int getTotalInstructions() const { return totalInstructions; }
    bool hasFinished() const { return remainingInstructions == 0; }
    std::string getName() const { return name; }
    int getId() const { return id; }
    std::string getTimeCreated() const { return timeCreated; }
    int getAssignedCore() const { return assignedCore; }
    
    void setAssignedCore(int core) { assignedCore = core; }
    const std::vector<std::string>& getLogs() const { return logs; }

    uint16_t getRequiredMemorySize() const { return requiredMemorySize; }

    void setInstructions(const std::vector<Instruction>& userInstructions) {
    instructions = userInstructions;
    totalInstructions = instructions.size();
    remainingInstructions = totalInstructions;

    }
};
//NEW
// Process* getProcessById(int pid) {
//     std::lock_guard<std::mutex> lock(processMutex);
//     for (auto* p : runningProcesses) {
//         if (p->getId() == pid) {
//             return p;
//         }
//     }
//     return nullptr;
// }

// void handlePageFault(Process* proc, int vpn, unsigned long long curCycle) {
//     int frame = -1;

//     if (!freeFrames.empty()) {
//         frame = freeFrames.front();
//         freeFrames.pop_front();
//     } else {
//         // LRU Selection
//         uint64_t oldest = UINT64_MAX;
//         int victim = -1;
//         for (int i = 0; i < frameTable.size(); ++i) {
//             if (frameTable[i].occupied && frameTable[i].lastUsed < oldest) {
//                 oldest = frameTable[i].lastUsed;
//                 victim = i;
//             }
//         }

//         if (victim != -1) {
//             auto& v = frameTable[victim];
//             auto procPtr = getProcessById(v.processId);
//             if (procPtr) {
//                 auto& victimPage = procPtr->getPage(v.virtualPage);
//                 if (victimPage.dirty) {
//                     numPagedOut++;
//                     writePageToBackingStore(v.processId, v.virtualPage, std::vector<int>(8, 0));
//                 }
//                 victimPage.present = false;
//                 victimPage.frameIndex = -1;
//             }
//             frame = victim;
//         }
//     }

//     // Page in
//     numPagedIn++;
//     frameTable[frame] = {proc->getId(), vpn, curCycle, true};

//     auto& newPage = proc->getPage(vpn);
//     newPage.present = true;
//     newPage.frameIndex = frame;
//     newPage.lastUsed = curCycle;
//     newPage.dirty = false;
// }

// bool accessMemory(Process* proc, int vpn, int curCycle) {
//     auto& entry = proc->getPage(vpn);
//     if (entry.present) {
//         frameTable[entry.frameIndex].lastUsed = curCycle;
//         entry.lastUsed = curCycle;
//         return true;
//     }
//     handlePageFault(proc, vpn, curCycle);
//     return true;
// }
//END OF NEW

// NEW: Function to dump memory status to a file
void printVMStat() {
    std::lock_guard<std::mutex> lock(processMutex);

    int totalMem = maxOverallMem;  // in bytes
    int usedMem = 0;

    for (const auto& p : runningProcesses) {
        if (p->getMemoryStartIndex() != -1) {
            usedMem += p->getFramesAllocated() * memPerFrame;
        }
    }

    int freeMem = totalMem - usedMem;
    int fragKB = countExternalFragmentation();

    // uint64_t idleTicks = 0, activeTicks = 0;
    // for (const auto& core : cpuCores) {
    //     idleTicks += core->getIdleTicks();
    //     activeTicks += core->getActiveTicks();
    // }

    std::cout << "\n=== VMSTAT ===\n";
    std::cout << "Total memory      : " << totalMem << " bytes\n";
    std::cout << "Used memory       : " << usedMem << " bytes\n";
    std::cout << "Free memory       : " << freeMem << " bytes\n";
    std::cout << "Total fragmentation: " << fragKB << " KB\n";
    // std::cout << "Idle CPU ticks    : " << idleTicks << "\n";
    // std::cout << "Active CPU ticks  : " << activeTicks << "\n";
    //std::cout << "Num paged in      : " << getTotalPagedIn() << "\n";
    //std::cout << "Num paged out     : " << getTotalPagedOut() << "\n";
    std::cout << "=================\n";
}



void cpuCycleLoop() {
    while (schedulerRunning) {
        global_simulated_cycles++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (global_simulated_cycles % 100 == 0) {
            std::this_thread::yield();
        }
    }
}

class Screen {
public:
    std::string screenName;
    int curInstruction;
    int totalInstruction;
    std::string timeCreated;
    bool isDetached;
    
    Screen() : isDetached(false) {}
};

std::vector<Screen> screenList;

// Utility functions
void setColor(int color) {
    switch(color) {
        case 7:  std::cout << "\033[37m"; break;  // White
        case 10: std::cout << "\033[32m"; break;  // Green
        case 14: std::cout << "\033[33m"; break;  // Yellow
        default: std::cout << "\033[37m"; break;  // Default to white
    }
}

void printASCII(std::string fileName) {
    std::string line = "";
    std::ifstream inFile;
    inFile.open(fileName);
    if(inFile.is_open()) {
        while(std::getline(inFile, line)) {
            std::cout << line << std::endl;
        }
    } else {
        std::cout << "File failed to load. " << std::endl;
    }
    inFile.close();
} 

// ASCII CSOPESY
void intro() {
    std::string fileName = "ascii.txt";
    printASCII(fileName);
    setColor(10);
    std::cout << "Hello, Welcome to CSOPESY commandline!" << std::endl;
    setColor(14);
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen." << std::endl;

    setColor(7);
    std::cout << "\nDevelopers:\n";
    std::cout << "Dimaculangan, Renzel\n"
            << "Guerrero, Miguel\n"
        << "Valdez, Kimi\n"
    << "Velasquez, Almira Zabrina Alyson\n\n";

    setColor(7);
    std::cout << "Last updated: ";
    setColor(14);
    std::cout << "31/07/2025" << std::endl;
    setColor(7);
    std::cout << "-------------------------------------------------------------------------" << std::endl;
}

// Abstract Scheduler base class
class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void addProcess(Process* process) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

// FCFS Scheduler class
//TODO add memory allocator to FCFSScheduler + generation of .txt file (not a prio for week 10)
class FCFSScheduler : public Scheduler {
private:
    int numCores; //Number of cores
    std::vector<std::thread> coreThreads;
    std::queue<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};

public:
    FCFSScheduler(int cores) : numCores(cores) {}
    
    ~FCFSScheduler() {
        stop();
    }
    
    void addProcess(Process* process) override {
        std::lock_guard<std::mutex> lock(queueMutex);
        processQueue.push(process);
        queueCV.notify_all();
    }
    
    void start() override {
        running = true;
        schedulerRunning = true;

        // Create threads for each core
        for (int i = 0; i < numCores; i++) {
            coreThreads.emplace_back([this, i]() {
                this->coreWorker(i);
            });
        }
        
        std::cout << "Scheduler started with " << numCores << " cores." << std::endl << std::endl;
    }
    
    void stop() override {

        running = false;
        schedulerRunning = false;
        queueCV.notify_all();
        
        for (auto& thread : coreThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        coreThreads.clear();
    }

    void coreWorker(int coreId) {
    while (running) {
        Process* processToExecute = nullptr;

        // get the process from the queue
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this]() { return !processQueue.empty() || !running; });

            if (!running) return;

            processToExecute = processQueue.front();
            processQueue.pop();
        }

        if (processToExecute->getMemoryStartIndex() == -1) {
            uint16_t memoryToAllocate = processToExecute->getRequiredMemorySize();
            if (memoryToAllocate == 0) {
                memoryToAllocate = getMemorySize();
            }

            int framesNeeded = memoryToAllocate / memPerFrame;
            int memIndex = allocateMemory(framesNeeded);

            if (memIndex == -1) {
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    processQueue.push(processToExecute);
                }
                queueCV.notify_all();
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // pause to prevent high CPU usage
                continue;
            }
            processToExecute->setMemoryAllocation(memIndex, framesNeeded);
        }
        
        // add to running list
        {
            std::lock_guard<std::mutex> pLock(processMutex);
            runningProcesses.push_back(processToExecute);
        }

        processToExecute->setAssignedCore(coreId);
        while (processToExecute->canExecute() && running) {
            processToExecute->executeInstruction(coreId);
        }
        if (processToExecute->getMemoryStartIndex() != -1) {
            freeMemory(processToExecute->getMemoryStartIndex(), processToExecute->getFramesAllocated());
        }

        // move to finished
        {
            std::lock_guard<std::mutex> pLock(processMutex);
            runningProcesses.erase(
                std::remove_if(runningProcesses.begin(), runningProcesses.end(),
                    [processToExecute](const Process* p) { return p->getId() == processToExecute->getId(); }),
                runningProcesses.end());

            finishedProcesses.push_back(processToExecute);
        }
    }
} 
    
    bool isRunning() const override { return running; }
};

//RR Scheduler Class
//TODO add memory allocator to FCFSScheduler + generation of .txt file
class RoundRobinScheduler : public Scheduler {
private:
    int numCores;
    int quantum;
    std::vector<std::thread> coreThreads;
    std::queue<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};
    std::set<int> activeProcessIds; // Track which processes are being executed

public:
    RoundRobinScheduler(int cores, int quantumCycles) : numCores(cores), quantum(quantumCycles) {}

    ~RoundRobinScheduler() {
        stop();
    }

    void addProcess(Process* process) override {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            processQueue.push(process);
        }
        queueCV.notify_one();
    }

    void start() override {
        running = true;
        schedulerRunning = true;

        for (int i = 0; i < numCores; ++i) {
            coreThreads.emplace_back([this, i]() {
                this->coreWorker(i);
            });
        }
        std::cout << "Round Robin Scheduler started with " << numCores << " cores and quantum of " << quantum << " cycles." << std::endl << std::endl;
    }

    void stop() override {
        running = false;
        schedulerRunning = false;
        queueCV.notify_all();

        for (auto& thread : coreThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        coreThreads.clear();
    }

    void coreWorker(int coreId) {
        while (running) {
            Process* processToExecute = nullptr;

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCV.wait(lock, [this] { return !processQueue.empty() || !running; });

                if (!running) return;

                processToExecute = processQueue.front();
                processQueue.pop();
                
                // Check if this process is already being executed by another core
                if (activeProcessIds.find(processToExecute->getId()) != activeProcessIds.end()) {
                    // Re-queue and try again
                    processQueue.push(processToExecute);
                    continue;
                }
                
                activeProcessIds.insert(processToExecute->getId());
            }

            {
                std::lock_guard<std::mutex> pLock(processMutex);
                if (std::find(runningProcesses.begin(), runningProcesses.end(), processToExecute) == runningProcesses.end()) {
                    runningProcesses.push_back(processToExecute);
                }
            }

            processToExecute->setAssignedCore(coreId);

            // Execute for quantum cycles
            int executedCycles = 0;
            while (executedCycles < quantum && processToExecute->canExecute() && running) {
                //NEW
                uint16_t memoryToAllocate = processToExecute->getRequiredMemorySize();
                if(processToExecute -> getMemoryStartIndex() == -1){
                    memPerProc = getMemorySize();
                    int framesNeeded = memoryToAllocate / memPerFrame;
                    int memIndex = allocateMemory(framesNeeded);
                    
                    if(memIndex == -1){
                        std::lock_guard<std::mutex> lock(queueMutex);
                        processQueue.push(processToExecute);
                        continue;
                    }
                    processToExecute->setMemoryAllocation(memIndex, framesNeeded);
                }
                processToExecute->executeInstruction(coreId);
                executedCycles++;
                
                // Small yield periodically to prevent CPU hogging
                if (executedCycles % 10 == 0) {
                    std::this_thread::yield();
                }
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                activeProcessIds.erase(processToExecute->getId());
            }

            if (processToExecute->hasFinished()) {
                std::lock_guard<std::mutex> pLock(processMutex);
                if(processToExecute->getMemoryStartIndex() != -1){
                    freeMemory(processToExecute->getMemoryStartIndex(), processToExecute->getFramesAllocated());
                }
                runningProcesses.erase(std::remove(runningProcesses.begin(), runningProcesses.end(), processToExecute), runningProcesses.end());
                finishedProcesses.push_back(processToExecute);
            } else if (running) {
                addProcess(processToExecute);
            }
        }
    }

    bool isRunning() const override { return running; }
};

void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}



void initialize() {
    if(readConfig() == true){
        initialized = true;
        std::cout << "-------------------------------------------------------------------------" << std::endl;
        std::cout << "Number of Cores: " << numCPU << std::endl;
        std::cout << "Scheduler Type: " << schedulerType << std::endl;
        std::cout << "Quantum Cycles: " << quantumCycles << std::endl;
        std::cout << "Batch Process Frequency: " << batchProcessFreq << std::endl;
        std::cout << "Minimum Instructions: " << minIns << std::endl;
        std::cout << "Maximum Instructions: " << maxIns << std::endl;
        std::cout << "Delay per Execution: " << delaysPerExec << std::endl;
        std::cout << "Max overall memory: " << maxOverallMem << std::endl;
        std::cout << "Memory per frame: " << memPerFrame << std::endl;
        std::cout << "Minimum Memory Per Process: " << minMemPerProc << std::endl;
        std::cout << "Maximum Memory Per Process: " << maxMemPerProc << std::endl;
        std::cout << "Total frames: " << totalFrames << std::endl;
        std::cout << "-------------------------------------------------------------------------" << std::endl;
        std::cout << "System Initialized. You may now create screens and perform other actions.\n\n";
    }
    else
        return;
    
}

Screen curScreen;

void createScreen(std::string &screenName, uint16_t memorySize) {
    std::time_t timestamp;
    std::time(&timestamp);

    Screen newScreen;
    newScreen.screenName = screenName;
    int instructionCount = minIns + (std::rand() % (maxIns - minIns + 1));
    newScreen.totalInstruction = instructionCount;
    newScreen.curInstruction = 0;
    
    char buffer[30];
    std::tm* timeinfo = std::localtime(&timestamp); 
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
    newScreen.timeCreated = buffer;
    newScreen.isDetached = false;

    screenList.emplace_back(newScreen);
    curScreen = newScreen;
    // Create corresponding process using unique_ptr
    auto newProcess = std::make_unique<Process>(screenName, instructionCount, memorySize);
    allProcesses.push_back(std::move(newProcess));
}

void manualProcessesScheduler() {
    if (scheduler && scheduler->isRunning()) {
        return;
    }

    if (scheduler == nullptr) {
        if (schedulerType == "fcfs") {
            scheduler = new FCFSScheduler(numCPU);
        } else if (schedulerType == "rr") {
            scheduler = new RoundRobinScheduler(numCPU, quantumCycles);
        } else {
            std::cout << "Error: Unknown scheduler type '" << schedulerType << "' in config.txt. Aborting." << std::endl;
            return;
        }
    }
    {
        std::lock_guard<std::mutex> pLock(processMutex);
        for (const auto& process : allProcesses) {
            if (!process->hasFinished()) {
                scheduler->addProcess(process.get());
            }
        }
    }
    schedulerRunning = true;
    std::thread(cpuCycleLoop).detach();
    scheduler->start();
    
}

// new!
void processSmi() {
    std::lock_guard<std::mutex> lock(processMutex);

    std::set<int> usedCores;
    for (const auto& process : runningProcesses) {
        if (process->getAssignedCore() != -1) {
            usedCores.insert(process->getAssignedCore());
        }
    }

    int coresUsed = usedCores.size();
    double cpuUtilization = (numCPU > 0) ? (static_cast<double>(coresUsed) / numCPU * 100.0) : 0.0;
    
    uint64_t totalMemoryUsed = 0;
    for (const auto& process : runningProcesses) {
        totalMemoryUsed += process->getRequiredMemorySize();
    }

    double memoryUtilization = (maxOverallMem > 0) ? (static_cast<double>(totalMemoryUsed) / maxOverallMem * 100.0) : 0.0;

    setColor(7);
    std::cout << "\n----------------------------------------------";
    std::cout << "\n| PROCESS-SMI V01.00 Driver Version: 01.00 |\n";
    std::cout << "----------------------------------------------\n";
    
    std::cout << std::fixed << std::setprecision(2);
    
    setColor(7);
    std::cout << "CPU-Util: ";
    setColor(14);
    std::cout << cpuUtilization << "%\n";
    
    setColor(7);
    std::cout << "Memory Usage: ";
    setColor(14);
    std::cout << totalMemoryUsed << "MiB / " << maxOverallMem << "MiB\n";
    
    setColor(7);
    std::cout << "Memory Util: ";
    setColor(14);
    std::cout << memoryUtilization << "%\n\n";
    
    setColor(7);
    std::cout << "=====================================\n";
    std::cout << "Running processes and memory usage:\n";
    std::cout << "----------------------------------------------\n";
    for (const auto& process : runningProcesses) {
        setColor(7);
        std::cout << process->getName() << " ";
        setColor(14);
        std::cout << process->getRequiredMemorySize() << "B\n";
    }
    setColor(7);
    std::cout << "----------------------------------------------";
}

void screenLS() {
    std::lock_guard<std::mutex> lock(processMutex);
    
    std::set<int> usedCores;
    for (const auto& process : runningProcesses) {
        if (process->getAssignedCore() != -1) {
            usedCores.insert(process->getAssignedCore());
        }
    }
    
    int coresUsed = usedCores.size();
    int coresAvailable = numCPU - coresUsed;
    double cpuUtilization = (numCPU > 0) ? (static_cast<double>(coresUsed) / numCPU * 100.0) : 0.0;
    
    setColor(7);
    std::cout << "CPU utilization: ";
    setColor(14);
    std::cout << std::fixed << std::setprecision(2) << cpuUtilization << "%\n";

    setColor(7);
    std::cout << "Cores used: ";
    setColor(14);
    std::cout << coresUsed << "\n";

    setColor(7);
    std::cout << "Cores available: ";
    setColor(14);
    std::cout << coresAvailable << "\n";

    setColor(7);
    std::cout << "================\n";
    std::cout << "Running processes:\n";
    
    for (const auto& process : runningProcesses) {
        setColor(7);
        std::cout << process->getName() << " (";
        setColor(14);
        std::cout << process->getTimeCreated();
        setColor(7);
        std::cout << ")  Core: ";
        setColor(14);
        std::cout << process->getAssignedCore();
        setColor(7);
        std::cout << "  ";
        setColor(14);
        std::cout << process->getCurrentInstruction();
        setColor(7);
        std::cout << " / ";
        setColor(14);
        std::cout << process->getTotalInstructions() << std::endl;
    }

    setColor(7);
    std::cout << "\n\n------------------\n\n";
    std::cout << "\nFinished processes:\n";

    for (const auto& process : finishedProcesses) {
        setColor(7);
        std::cout << process->getName() << "  (";
        setColor(14);
        std::cout << process->getTimeCreated();
        setColor(7);
        std::cout << ")  Finished ";
        setColor(14);
        std::cout << process->getTotalInstructions() << " / " << process->getTotalInstructions() << std::endl;
    }

    setColor(7);
    std::cout << "================\n\n";
}



void screen(std::string &screenCommand) {
    std::istringstream iss(screenCommand);
    std::string command, option, argument, memStr;
    iss >> command >> option >> argument;

    if(option == "-s" && !argument.empty()) {
        //std::vector<Process*> justCreated; // A temporary list to hold newly created processes

        iss >> memStr;

        if (argument.empty() || memStr.empty()) {
            std::cout << "Usage: screen -s <process_name> <process_memory_size>" << std::endl;
            return;
        }

        uint16_t memSize;
        try {
            memSize = std::stoul(memStr);
        } catch (...) {
            std::cout << "Invalid memory size format." << std::endl;
            return;
        }
        
        if (!isValidMemorySize(memSize)) {
            std::cout << "invalid memory allocation" << std::endl;
            return;
        }

        createScreen(argument, memSize);
        Process* newProcess = allProcesses.back().get();

        if (scheduler && scheduler->isRunning()) {
            scheduler->addProcess(newProcess);
        } else {
            manualProcessesScheduler();
        }

        clearScreen();
        std::cout << "Screen created: " << newProcess->getName() << std::endl;
        std::cout << "Instructions: " << newProcess->getCurrentInstruction() << " out of " << newProcess->getTotalInstructions() << std::endl;
        std::cout << "Time Created: " << newProcess->getTimeCreated() << std::endl;

        std::string screenInput;
        while (true) {
            std::cout << "\n" << newProcess->getName() << ":\\> ";
            std::getline(std::cin, screenInput);

            if (screenInput == "process-smi") {
                Process* process = nullptr;
                auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                    [&](const std::unique_ptr<Process>& p) { return p->getName() == newProcess->getName(); });

                if (it != allProcesses.end()) {
            process = it->get();

            setColor(7);
            std::cout << "\nProcess name: " << process->getName();

            if (process->hasFinished()) {
            std::cout << " Finished!" << std::endl;
                } else {
                    std::cout << std::endl;
                }

                setColor(7);
                std::cout << "ID: ";
                setColor(14);
                std::cout << process->getId() << std::endl;

                setColor(7);
                std::cout << "Time Created: ";
                setColor(14);
                std::cout << process->getTimeCreated() << std::endl;

                setColor(7);
                std::cout << "Assigned Core: ";
                setColor(14);
                std::cout << process->getAssignedCore() << std::endl;

                setColor(7);
                std::cout << "Logs:" << std::endl;
                for (const auto& logEntry : process->getLogs()) {
                    if (logEntry.find("Value of") != std::string::npos || logEntry.find("Hello world from") != std::string::npos) {
                        std::cout << logEntry << std::endl;
                    }
                }

                
                setColor(7);
                std::cout << "\nCurrent instruction line: ";
                setColor(14);
                std::cout << process->getCurrentInstruction() << std::endl;

                setColor(7);
                std::cout << "Lines of code: ";
                setColor(14);
                std::cout << process->getTotalInstructions() << "\n";

                setColor(7);
            }

            } else if (screenInput == "exit") {
                clearScreen();
                intro();
                break;   
            } else {
                std::cout << "Invalid command. You can only use 'process-smi' or 'exit'." << std::endl;
            }
        }
        return;
    }
    else if(option == "-r" && !argument.empty()) {
        std::string screenName = argument;
        bool found = false;
        
        auto finishedIt = std::find_if(finishedProcesses.begin(), finishedProcesses.end(),
            [&screenName](const Process* p) { return p->getName() == screenName; });
        
        if (finishedIt != finishedProcesses.end()) {
            std::cout << "Process \"" << screenName << "\" has already finished execution and cannot be accessed." << std::endl << std::endl;
            return;
        }
        
        for(auto& scr : screenList) {
            if(scr.screenName == screenName) {
                clearScreen();

                curScreen = scr;
                std::cout << "Screen: " << curScreen.screenName << std::endl;
                auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                    [&screenName](const std::unique_ptr<Process>& p) { return p->getName() == screenName; });
                
                if (it != allProcesses.end()) {
                    std::cout << "Running instruction: " << (*it)->getCurrentInstruction() 
                            << " out of " << (*it)->getTotalInstructions() << std::endl;
                } else {
                    std::cout << "Running instruction: " << curScreen.curInstruction 
                            << " out of " << curScreen.totalInstruction << std::endl;
                }
                
                std::cout << "Time Created: " << curScreen.timeCreated << std::endl << std::endl;
                scr.isDetached = false;
                found = true;
                break;
            }
        }
        if(!found) {
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl << std::endl;
            return;
        }

        if (found) {
            std::string screenInput;
            while(screenInput != "exit") {
                std::cout << "\nroot:\\> ";
                std::getline(std::cin, screenInput);
                if(screenInput == "process-smi") {
                    auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                        [&screenName](const std::unique_ptr<Process>& p) { return p->getName() == screenName; });
                    
                    if (it != allProcesses.end()) {
                        Process* process = it->get();
                        std::cout << "\nProcess name: " << process->getName();
                        
                        if (process->hasFinished()) {
                            std::cout << " Finished!" << std::endl;
                        } else {
                            std::cout << std::endl;
                        }
                        
                        std::cout << "ID: " << process->getId() << std::endl;
                        std::cout << "Logs:" << std::endl;
                        for (const auto& logEntry : process->getLogs()) {
                            if (logEntry.find("Hello world from") != std::string::npos) {
                                std::cout << logEntry << std::endl;
                            }
                        }
                        std::cout << "\nCurrent instruction line: " << process->getCurrentInstruction() << std::endl;
                        std::cout << "Lines of code: " << process->getTotalInstructions() << "\n" << std::endl;
                    }   
                }
                else if (screenInput == "exit"){
                    clearScreen();
                    intro();
                    break; 
                }
                else{
                    std::cout << "Invalid command. You can only input 'process-smi' or 'exit'. " << std::endl << std::endl;
                }
            }
        }
    }
    else if (option == "-c" && !argument.empty()) {
        std::string processName = argument;
        std::string memStr;
        iss >> memStr;

        if (memStr.empty()) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        uint16_t memSize;
        try {
            memSize = static_cast<uint16_t>(std::stoi(memStr));
        } catch (...) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        if (!isValidMemorySize(memSize)) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        // Get the remaining string (should be quoted)
        std::string instructionsString;
        std::getline(iss >> std::ws, instructionsString);

        if (instructionsString.length() >= 2 && instructionsString.front() == '"' && instructionsString.back() == '"') {
            instructionsString = instructionsString.substr(1, instructionsString.length() - 2);
        } else {
            std::cout << "invalid command: instructions must be enclosed in double quotes" << std::endl;
            return;
        }
        
        if (instructionsString.empty()) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        std::vector<Instruction> parsed = parseUserInstructions(instructionsString);
        if (parsed.size() < 1 || parsed.size() > 50) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        createScreen(processName, memSize);
        Process* newProcess = allProcesses.back().get();
        //newProcess -> initPageTable(totalFrames);
        newProcess->setInstructions(parsed);

        if (scheduler && scheduler->isRunning()) {
            scheduler->addProcess(newProcess);
        } else {
            manualProcessesScheduler();
        }

        //clearScreen();
        std::cout << "Screen created: " << newProcess->getName() << std::endl;
        std::cout << "Instructions: " << newProcess->getCurrentInstruction() << " out of " << newProcess->getTotalInstructions() << std::endl;
        std::cout << "Time Created: " << newProcess->getTimeCreated() << std::endl;

        std::string screenInput;
        while (true) {
            std::cout << "\n" << newProcess->getName() << ":\\> ";
            std::getline(std::cin, screenInput);

            if (screenInput == "process-smi") {
                Process* process = nullptr;
                auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                    [&](const std::unique_ptr<Process>& p) { return p->getName() == newProcess->getName(); });

                if (it != allProcesses.end()) {
                    process = it->get();

                    setColor(7);
                    std::cout << "\nProcess name: " << process->getName();

                    if (process->hasFinished()) {
                        std::cout << " Finished!" << std::endl;
                    } else {
                        std::cout << std::endl;
                    }

                    setColor(7);
                    std::cout << "ID: ";
                    setColor(14);
                    std::cout << process->getId() << std::endl;

                    setColor(7);
                    std::cout << "Time Created: ";
                    setColor(14);
                    std::cout << process->getTimeCreated() << std::endl;

                    setColor(7);
                    std::cout << "Assigned Core: ";
                    setColor(14);
                    std::cout << process->getAssignedCore() << std::endl;

                    setColor(7);
                    std::cout << "Logs:" << std::endl;
                    for (const auto& logEntry : process->getLogs()) {
                        std::cout << logEntry << std::endl;
                    }

                    setColor(7);
                    std::cout << "\nCurrent instruction line: ";
                    setColor(14);
                    std::cout << process->getCurrentInstruction() << std::endl;

                    setColor(7);
                    std::cout << "Lines of code: ";
                    setColor(14);
                    std::cout << process->getTotalInstructions() << "\n";

                    setColor(7);
                }
            } else if (screenInput == "exit") {
                clearScreen();
                intro();
                break;
            } else {
                std::cout << "Invalid command. You can only use 'process-smi' or 'exit'." << std::endl;
            }
        }
    }
    else if(option == "-d" && !argument.empty()) {
        std::string screenName = argument;
        bool found = false;
        for(auto& scr : screenList) {
            if(scr.screenName == screenName) {
                scr.isDetached = true;
                std::cout << "Screen \"" << screenName << "\" detached." << std::endl << std::endl;
                found = true;
                break;
            }
        }
        if(!found) {
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl << std::endl;
        }
    }
    else if(option == "-ls") {
        screenLS();
        return;
    }
}

void processCreationLoop() {
    static int i = 0;
    long long lastCycle = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (processCreationRunning) {
        if (scheduler && scheduler->isRunning()) {
            int coresInUse = 0;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                std::set<int> usedCores;
                for (const auto& process : runningProcesses) {
                    if (process->getAssignedCore() != -1) {
                        usedCores.insert(process->getAssignedCore());
                    }
                }
                coresInUse = usedCores.size();
            }

            int activeProcessCount = 0;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                activeProcessCount = runningProcesses.size();
            }
            if (coresInUse < numCPU && (global_simulated_cycles - lastCycle >= batchProcessFreq)) {
                std::string processName = "process_";
                if (i < 10) {
                    processName += "0";
                }
                processName += std::to_string(i++);

                createScreen(processName, 0);
                scheduler->addProcess(allProcesses.back().get());

                lastCycle = global_simulated_cycles;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void schedulerStart() {
    if (scheduler == nullptr) {
        if (schedulerType == "fcfs") {
            scheduler = new FCFSScheduler(numCPU);
        } else if (schedulerType == "rr") {
            scheduler = new RoundRobinScheduler(numCPU, quantumCycles);
        } else {
            std::cout << "Error: Unknown scheduler type '" << schedulerType << "' in config.txt. Aborting." << std::endl;
            return;
        }
    }

    if (!scheduler->isRunning()) {

        {
            std::lock_guard<std::mutex> pLock(processMutex);
            for (const auto& process : allProcesses) {
                if (!process->hasFinished()) {
                    scheduler->addProcess(process.get());
                }
            }
        }
        
        std::cout << "Creating processes. Enter \"scheduler-stop\" to cease." << std::endl;
        processCreationRunning = true;
        schedulerRunning = true;
        std::thread(cpuCycleLoop).detach();         // Start CPU cycle simulation
        std::thread(processCreationLoop).detach();  // Start process creation in background
        scheduler->start();
    } else {
        std::cout << "Scheduler is already running." << std::endl << std::endl;
    }
}

/*void schedulerStop() {
    std::cout << "Stopping new process creation..." << std::endl;
    std::cout << "Total screens/processes created: " << screenList.size() << std::endl;
    processCreationRunning = false; // no new processes

    std::thread([]() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(processMutex);
                if (runningProcesses.empty()) {
                    if (scheduler != nullptr && scheduler->isRunning()) {
                        scheduler->stop();
                    }
                    schedulerRunning = false; // scheduler is done
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();

    
}
*/

void schedulerStop() {
    std::cout << "Stopping new process creation..." << std::endl;
    std::cout << "Total screens/processes created: " << screenList.size() << std::endl;
    
    processCreationRunning = false;

    // Detach any dangling CPU/process creation loops (if running)
    std::thread([]() {
        while (true) {
            bool allDone = true;

            {
                std::lock_guard<std::mutex> lock(processMutex);
                for(const auto& proc : allProcesses) {
                    if (!proc->hasFinished()) {
                        allDone = false;
                        break;
                    }
                }
            }

            if (allDone) {
                if (scheduler != nullptr && scheduler->isRunning()) {
                    scheduler->stop();
                }

                schedulerRunning = false;
                std::cout << "\nAll processes are done running.\n";  // Explicit final log
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
}



void reportUtil() {    
    std::ofstream outFile("csopesy-log.txt");
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing.\n";
        return;
    }

    std::lock_guard<std::mutex> lock(processMutex);

    std::set<int> usedCores;
    for (const auto& process : runningProcesses) { 
        if (process->getAssignedCore() != -1) {
            usedCores.insert(process->getAssignedCore());
        }
    }
    
    int coresUsed = usedCores.size();
    int coresAvailable = numCPU - coresUsed;
    double cpuUtilization = (numCPU > 0) ? (static_cast<double>(coresUsed) / numCPU * 100.0) : 0.0;
    
    outFile << "CPU utilization: " << std::fixed << std::setprecision(2) << cpuUtilization << "%\n";
    outFile << "Cores used: " << coresUsed << "\n";
    outFile << "Cores available: " << coresAvailable << "\n";

    outFile << "================\n";
    outFile << "Running processes:\n";

    for (const auto& process : runningProcesses) {
        outFile << process->getName() << " (" << process->getTimeCreated() << ")  "
                << "Core: " << process->getAssignedCore() << "  "
                << process->getCurrentInstruction() << "  /  " << process->getTotalInstructions() << "\n";
    }

    outFile << "\n\n------------------\n\n";

    outFile << "\nFinished processes:\n";
    for (const auto& process : finishedProcesses) {
        outFile << process->getName() << "  (" << process->getTimeCreated() << ")  "
                << "Finished " << process->getTotalInstructions() << "  /  " << process->getTotalInstructions() << "\n";
    }

    outFile << "================\n\n";

    outFile.close();
    std::cout << "Report generated at opesy-log.txt!\n\n";


}


void menu() {
    std::string input;
    bool menuRunning = true;
    intro();
    while (menuRunning) {
        
        setColor(7);
        std::cout << "\nroot:\\> ";
        std::string command;
        std::getline(std::cin >> std::ws, command);

        if(command == "exit") {
            if (scheduler != nullptr) {
                scheduler->stop();
                delete scheduler;
            }
            menuRunning = false;
            std::cout << "Thank you for using the program";
            exit(0);
        }
        else if (command == "clear") {
            clearScreen();
            intro();
        }
        else if(command == "initialize") {
            initialize();
        }
        else if (!initialized) {
            std::cout << "Command is not recognized. Please initialize the system first by using the 'initialize' command.\n\n";
        }
        else if(command.rfind("screen",0) == 0) {
            screen(command);
        }
        else if(command == "scheduler-start") {
            schedulerStart();
        }
        else if(command == "scheduler-stop") {
            schedulerStop();
        }
        else if(command == "report-util") {
            reportUtil();
        }

        else if(command == "process-smi"){
            processSmi();
        }
        else if(command == "vmstat"){
            printVMStat();
        }
        else {
            std::cout << "Unknown command. Please try again.\n\n";
        }
    }
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    menu();
    return 0;
}