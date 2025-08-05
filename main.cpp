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


class Process;
class Scheduler;
class FCFSScheduler;
class RoundRobinScheduler;
bool accessMemory(Process *proc, int vpn, int curCycle);
void handlePageFault(Process *proc, int vpn, unsigned long long curCycle);

struct FrameEntry
{
    bool occupied = false;
    int processId = -1;
    int virtualPageNum = -1;
    uint64_t lastUsedTimestamp = 0;
    std::vector<uint8_t> data;
};

std::vector<FrameEntry> frameTable; 
std::deque<int> freeFrameList;      


std::atomic<uint64_t> numPagedIn{0};
std::atomic<uint64_t> numPagedOut{0};
std::atomic<uint64_t> totalIdleTicks{0};
std::atomic<uint64_t> totalActiveTicks{0};
std::atomic<bool> systemRunning{true}; 



std::list<std::unique_ptr<Process>> allProcesses;
std::vector<Process *> runningProcesses;
std::vector<Process *> finishedProcesses;
Scheduler *scheduler = nullptr;
std::mutex processMutex;


std::mutex memoryMutex;

std::atomic<bool> schedulerRunning{false};
std::atomic<int> processIdCounter{1};
std::atomic<long long> global_simulated_cycles{0}; 
std::atomic<bool> processCreationRunning{false};
std::atomic<int> autoProcessCounter{0}; 





int numCPU;                
std::string schedulerType; 
uint64_t quantumCycles;    
uint64_t batchProcessFreq; 
uint64_t minIns;
uint64_t maxIns;
uint64_t delaysPerExec; 
uint16_t maxOverallMem;
uint16_t memPerFrame;
uint16_t memPerProc;
uint16_t minMemPerProc; 
uint16_t maxMemPerProc; 
uint16_t totalFrames = 0;
bool initialized = false;


enum class InstructionType
{
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

struct Instruction
{
    InstructionType type;
    std::vector<std::string> args;
    std::vector<Instruction> nestedInstructions;
};


std::vector<Instruction> parseUserInstructions(const std::string &input)
{
    std::vector<Instruction> result;
    std::istringstream stream(input);
    std::string token;

    std::cout << "[DEBUG] Full input string: [" << input << "]\n";

    
    while (std::getline(stream, token, ';'))
    {
        std::cout << "[DEBUG] Processing token: [" << token << "]\n";

        
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);

        if (token.empty())
        {
            std::cout << "[DEBUG] Empty token, skipping\n";
            continue;
        }

        std::string op;
        size_t op_end = token.find_first_of(" (");
        if (op_end != std::string::npos)
        {
            op = token.substr(0, op_end);
        }
        else
        {
            op = token;
        }

        Instruction instr;
        instr.args.clear();
        std::cout << "[DEBUG] Operation: [" << op << "]\n";

        if (op == "PRINT")
        {
            instr.type = InstructionType::PRINT;

            
            size_t start = token.find('(');
            size_t end = token.rfind(')');

            std::cout << "[DEBUG] PRINT token: [" << token << "]\n";
            std::cout << "[DEBUG] Parentheses at: " << start << " and " << end << "\n";

            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                std::string inside = token.substr(start + 1, end - start - 1);
                std::cout << "[DEBUG] Inside parentheses: [" << inside << "]\n";

                
                std::string cleaned;
                bool escape = false;
                for (char ch : inside)
                {
                    if (escape)
                    {
                        if (ch == '\"')
                            cleaned += '\"';
                        else
                        {
                            cleaned += '\\';
                            cleaned += ch;
                        }
                        escape = false;
                    }
                    else if (ch == '\\')
                    {
                        escape = true;
                    }
                    else
                    {
                        cleaned += ch;
                    }
                }
                if (escape)
                    cleaned += '\\';

                
                cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
                cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);

                instr.args.push_back(cleaned);
                std::cout << "[DEBUG] Parsed PRINT argument: [" << cleaned << "]\n";
            }
            else
            {
                std::cerr << "Warning: Malformed PRINT instruction: " << token << std::endl;
                continue;
            }
        }
        else if (op == "DECLARE")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string var, value;
            instrStream >> var >> value;
            if (var.empty() || value.empty())
            {
                std::cerr << "Warning: Invalid DECLARE instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::DECLARE;
            instr.args = {var, value};
            std::cout << "[DEBUG] Parsed DECLARE: " << var << " = " << value << "\n";
        }
        else if (op == "ADD")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty())
            {
                std::cerr << "Warning: Invalid ADD instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::ADD;
            instr.args = {a, b, c};
            std::cout << "[DEBUG] Parsed ADD: " << a << " = " << b << " + " << c << "\n";
        }
        else if (op == "SUBTRACT")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty())
            {
                std::cerr << "Warning: Invalid SUBTRACT instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::SUBTRACT;
            instr.args = {a, b, c};
        }
        else if (op == "WRITE")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string addr, var;
            instrStream >> addr >> var;
            if (addr.empty() || var.empty())
            {
                std::cerr << "Warning: Invalid WRITE instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::WRITE;
            instr.args = {addr, var};
        }
        else if (op == "READ")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string var, addr;
            instrStream >> var >> addr;
            if (var.empty() || addr.empty())
            {
                std::cerr << "Warning: Invalid READ instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::READ;
            instr.args = {var, addr};
        }
        else if (op == "SLEEP")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string ticks;
            instrStream >> ticks;
            if (ticks.empty())
            {
                std::cerr << "Warning: Invalid SLEEP instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::SLEEP;
            instr.args = {ticks};
        }
        else if (op == "MULTIPLY")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty())
            {
                std::cerr << "Warning: Invalid MULTIPLY instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::MULTIPLY;
            instr.args = {a, b, c};
        }
        else if (op == "DIVIDE")
        {
            std::istringstream instrStream(token.substr(op.length()));
            std::string a, b, c;
            instrStream >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty())
            {
                std::cerr << "Warning: Invalid DIVIDE instruction: " << token << std::endl;
                continue;
            }
            instr.type = InstructionType::DIVIDE;
            instr.args = {a, b, c};
        }
        else
        {
            std::cerr << "Warning: Unknown instruction type: " << op << std::endl;
            continue;
        }

        std::cout << "[DEBUG] Adding instruction: " << op << std::endl;
        result.push_back(instr);
    }

    std::cout << "[DEBUG] Total instructions parsed: " << result.size() << std::endl;
    return result;
}

bool isValidMemorySize(uint16_t num)
{
    
    return (num >= 2) && (num <= 65536) && ((num & (num - 1)) == 0);
}

uint16_t getMemorySize()
{
    int minExponent = static_cast<int>(std::log2(minMemPerProc));
    int maxExponent = static_cast<int>(std::log2(maxMemPerProc));
    int range = maxExponent - minExponent + 1;
    int randomExponent = minExponent + (std::rand() % range);
    return static_cast<uint16_t>(1 << randomExponent);
}

bool readConfig()
{
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

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "num-cpu")
        {
            iss >> numCPU;
            if (numCPU < 1 || numCPU > 128)
            {
                outOfRangeCPU = true;
                std::cout << "Error: num-cpu must be between 1 and 128 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "scheduler")
        {
            iss >> std::quoted(schedulerType);
            if (schedulerType != "fcfs" && schedulerType != "rr")
            {
                outOfRangeScheduler = true;
                std::cout << "Error: scheduler can only be fcfs or rr. Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "quantum-cycles")
        {
            iss >> quantumCycles;
            if (quantumCycles < 0 || quantumCycles > 4294967296)
            {
                outOfRangeQuantum = true;
                std::cout << "Error: quantum-cycles must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "batch-process-freq")
        {
            iss >> batchProcessFreq;
            if (batchProcessFreq < 1 || batchProcessFreq > 4294967296)
            {
                outOfRangeBatch = true;
                std::cout << "Error: batch-process-freq must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "min-ins")
        {
            iss >> minIns;
            if (minIns < 1 || minIns > 4294967296)
            {
                outOfRangeMin = true;
                std::cout << "Error: min-ins must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "max-ins")
        {
            iss >> maxIns;
            if (maxIns < 1 || maxIns > 4294967296)
            {
                outOfRangeMax = true;
                std::cout << "Error: max-ins must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
            if (maxIns < minIns)
            {
                outOfRangeMax = true;
                std::cout << "Error: max-ins must be greater than or equal to min-ins. Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "delays-per-exec")
        {
            iss >> delaysPerExec;
            if (delaysPerExec < 0 || delaysPerExec > 4294967296)
            {
                outOfRangeDelay = true;
                std::cout << "Error: delays-per-exec must be between 0 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "max-overall-mem")
        {
            iss >> maxOverallMem;
            if (maxOverallMem < 1 || maxOverallMem > 4294967296)
            {
                outOfRangeMaxMem = true;
                std::cout << "Error: max-overall-mem must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "mem-per-frame")
        {
            iss >> memPerFrame;
            if (memPerFrame < 1 || memPerFrame > 4294967296)
            {
                outOfRangeMemPerFrame = true;
                std::cout << "Error: mem-per-frame must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "min-mem-per-proc")
        {
            iss >> minMemPerProc;
            if (((isValidMemorySize(minMemPerProc)) == false))
            {
                outOfRangeMinMemPerProc = true;
                std::cout << "Error: min-mem-per-proc must be a power of two between 64 and 65536 (inclusive). Please reconfigure config.txt." << std::endl;
            }
            if (minMemPerProc > maxOverallMem)
            {
                outOfRangeMinMemPerProc = true;
                std::cout << "Error: min-mem-per-proc must be less than or equal to max-overall-mem. Please reconfigure config.txt." << std::endl;
            }
        }
        else if (key == "max-mem-per-proc")
        {
            iss >> maxMemPerProc;
            if ((isValidMemorySize(maxMemPerProc)) == false)
            {
                outOfRangeMaxMem = true;
                std::cout << "Error: max-mem-per-proc must be a power of two between 64 and 65536 (inclusive). Please reconfigure config.txt." << std::endl;
            }
            if (maxMemPerProc < minMemPerProc)
            {
                outOfRangeMaxMemPerProc = true;
                std::cout << "Error: max-mem-per-proc must be greater than or equal to min-mem-per-proc. Please reconfigure config.txt." << std::endl;
            }
            if (maxMemPerProc > maxOverallMem)
            {
                outOfRangeMaxMemPerProc = true;
                std::cout << "Error: max-mem-per-proc must be less than or equal to max-overall-mem. Please reconfigure config.txt." << std::endl;
            }
        }
    }
    if (outOfRangeCPU || outOfRangeScheduler || outOfRangeQuantum || outOfRangeBatch || outOfRangeMin || outOfRangeMax || outOfRangeDelay || outOfRangeMaxMem || outOfRangeMemPerFrame || outOfRangeMinMemPerProc || outOfRangeMaxMemPerProc)
        return false;
    totalFrames = maxOverallMem / memPerFrame;
    
    return true;
}

std::vector<uint8_t> readPageFromBackingStore(int pid, int vpn)
{
    std::ifstream in("csopesy-backing-store.txt");
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream iss(line);
        int filePid, fileVpn;
        char colon; 
        iss >> filePid >> fileVpn >> colon;

        if (filePid == pid && fileVpn == vpn)
        {
            std::vector<uint8_t> data;
            int val;
            while (iss >> val)
            {
                data.push_back(static_cast<uint8_t>(val));
            }
            
            data.resize(memPerFrame, 0);
            return data;
        }
    }
    
    return std::vector<uint8_t>(memPerFrame, 0);
}


void writePageToBackingStore(int pid, int vpn, const std::vector<uint8_t> &data)
{
    const std::string filename = "csopesy-backing-store.txt";
    std::vector<std::string> lines;
    std::string line;
    bool pageFound = false;

    
    std::ifstream inFile(filename);
    while (std::getline(inFile, line))
    {
        lines.push_back(line);
    }
    inFile.close();

    
    std::ostringstream new_line_ss;
    new_line_ss << pid << " " << vpn << " :";
    for (const auto &byte : data)
    {
        new_line_ss << " " << static_cast<int>(byte);
    }
    std::string new_line = new_line_ss.str();

    
    for (auto &l : lines)
    {
        std::istringstream iss(l);
        int filePid, fileVpn;
        iss >> filePid >> fileVpn;
        if (filePid == pid && fileVpn == vpn)
        {
            l = new_line;
            pageFound = true;
            break;
        }
    }

    
    if (!pageFound)
    {
        lines.push_back(new_line);
    }

    
    std::ofstream outFile(filename, std::ios::trunc);
    for (const auto &l : lines)
    {
        outFile << l << std::endl;
    }
    outFile.close();
}




class Process
{
    friend bool accessMemory(Process *proc, int vpn, int curCycle);
    friend void handlePageFault(Process *proc, int vpn, unsigned long long curCycle);

private:
    std::string name;
    int id;
    int totalInstructions;
    int remainingInstructions;
    int currentInstruction;
    std::string timeCreated;
    int assignedCore;
    std::unique_ptr<std::ofstream> logFile;
    std::mutex processExecutionMutex;
    std::vector<std::string> logs;
    std::atomic<int> cyclesSinceLastExec{0};
    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    std::atomic<uint64_t> sleepUntilCycle{0};
    std::vector<int> forLoopCounters;
    std::vector<int> forLoopMaxRepeats;

    uint16_t requiredMemorySize;

    struct PageTableEntry
    {
        bool present = false;
        bool dirty = false;
        int frameIndex = -1;
    };
    std::vector<PageTableEntry> pageTable;

    bool terminatedDueToMemoryViolation = false;
    std::string violationTimestamp;
    uint32_t violationAddress = 0;

public:
    
    
    Process(const std::string &processName, int numInstructions = 100, uint16_t memSize = 0)
        : name(processName), totalInstructions(numInstructions),
          remainingInstructions(numInstructions), currentInstruction(0), assignedCore(-1), requiredMemorySize(memSize)
    {

        id = processIdCounter++;

        
        std::time_t timestamp;
        std::time(&timestamp);
        char buffer[30];
        std::tm *timeinfo = std::localtime(&timestamp);
        std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
        timeCreated = buffer;

        if (memPerFrame > 0)
        {
            int numPages = (requiredMemorySize + memPerFrame - 1) / memPerFrame;
            pageTable.resize(numPages);
        }

        generateRandomInstructions(numInstructions);
    }

    
    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;

    
    Process(Process &&other) noexcept
        : name(std::move(other.name)), id(other.id), totalInstructions(other.totalInstructions),
          remainingInstructions(other.remainingInstructions), currentInstruction(other.currentInstruction),
          timeCreated(std::move(other.timeCreated)), assignedCore(other.assignedCore),
          logFile(std::move(other.logFile)), instructions(std::move(other.instructions)),
          variables(std::move(other.variables)),
          cyclesSinceLastExec(other.cyclesSinceLastExec.load()),
          sleepUntilCycle(other.sleepUntilCycle.load())
    {
    }

    
    Process &operator=(Process &&other) noexcept
    {
        if (this != &other)
        {
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

    
    ~Process()
    {
        if (logFile && logFile->is_open())
        {
            logFile->close();
        }
    }
    
    
    
    
    
    

    

    
    
    

    
    
    

    
    
    void generateRandomInstructions(int numInstructions, int depth = 0)
    {
        instructions.clear();
        for (int i = 0; i < numInstructions; ++i)
        {
            Instruction instr;
            int instrType = std::rand() % 6;
            switch (instrType)
            {
            case 0: 
                instr.type = InstructionType::PRINT;
                instr.args.push_back("\"Hello world from " + name + "!\"");
                break;
            case 1: 
                instr.type = InstructionType::DECLARE;
                instr.args.push_back("var" + std::to_string(std::rand() % 10));
                instr.args.push_back(std::to_string(std::rand() % 65536));
                break;
            case 2: 
                instr.type = InstructionType::ADD;
                instr.args.push_back("var" + std::to_string(std::rand() % 10));
                instr.args.push_back("var" + std::to_string(std::rand() % 10));
                instr.args.push_back(std::to_string(std::rand() % 65536));
                break;
            case 3: 
                instr.type = InstructionType::SUBTRACT;
                instr.args.push_back("var" + std::to_string(std::rand() % 10));
                instr.args.push_back("var" + std::to_string(std::rand() % 10));
                instr.args.push_back(std::to_string(std::rand() % 65536));
                break;
            case 4: 
                instr.type = InstructionType::SLEEP;
                instr.args.push_back(std::to_string(std::rand() % 255 + 1)); 
                break;
            case 5: 
                if (depth < 3)
                { 
                    instr.type = InstructionType::FOR;
                    int repeats = std::rand() % 5 + 1;
                    instr.args.push_back(std::to_string(repeats));
                    int nestedInstructionCount = std::rand() % 3 + 1;
                    for (int j = 0; j < nestedInstructionCount; ++j)
                    {
                        
                        Instruction nested;
                        nested.type = InstructionType::PRINT;
                        nested.args.push_back("\"Nested loop says hi!\"");
                        instr.nestedInstructions.push_back(nested);
                    }
                }
                else
                { 
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

    
    void recordMemoryViolation(uint32_t address)
    {
        terminatedDueToMemoryViolation = true;
        violationAddress = address;

        
        std::time_t timestamp;
        std::time(&timestamp);
        char buffer[30];
        std::tm *timeinfo = std::localtime(&timestamp);
        std::strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
        violationTimestamp = buffer;

        
        remainingInstructions = 0;
        currentInstruction = instructions.size(); 
    }

    bool hasMemoryViolation() const { return terminatedDueToMemoryViolation; }
    std::string getViolationTimestamp() const { return violationTimestamp; }
    uint32_t getViolationAddress() const { return violationAddress; }

    
    bool isValidMemoryAddress(uint32_t address) const
    {
        if (memPerFrame == 0)
            return false; 
        uint32_t maxVirtualAddress = pageTable.size() * memPerFrame;
        return (address < maxVirtualAddress);
    }

    
    bool executeInstruction(int coreId)
    {
        std::lock_guard<std::mutex> lock(processExecutionMutex);

        if (global_simulated_cycles < sleepUntilCycle)
        {
            return true;
        }

        if (delaysPerExec > 0)
        {
            cyclesSinceLastExec++;
            if (cyclesSinceLastExec <= delaysPerExec)
            {
                return true;
            }
            cyclesSinceLastExec = 0;
        }

        if (currentInstruction >= instructions.size())
        {
            return false;
        }

        const auto &instr = instructions[currentInstruction];
        assignedCore = coreId;
        std::ostringstream oss;

        switch (instr.type)
        {
        case InstructionType::PRINT:
        {
            std::string msg = instr.args[0];
            
            size_t pos = msg.find("+");
            if (pos != std::string::npos)
            {
                std::string varName = msg.substr(pos + 1);
                
                varName.erase(std::remove_if(varName.begin(), varName.end(), ::isspace), varName.end());
                msg = msg.substr(0, pos);
                oss << msg.substr(1, msg.length() - 2) << (variables.count(varName) ? std::to_string(variables[varName]) : "0");
            }
            else
            {
                oss << msg.substr(1, msg.length() - 2);
            }
            break;
        }
        
        case InstructionType::DECLARE:
        {
            
            accessMemory(this, 0, global_simulated_cycles);

            if (variables.size() >= 32)
            {
                oss << "Symbol table full." << instr.args[0];
            }
            else
            {
                variables[instr.args[0]] = static_cast<uint16_t>(std::stoul(instr.args[1]));
                oss << "Declared " << instr.args[0] << " = " << instr.args[1];

                
                pageTable[0].dirty = true;
            }
            break;
        }
        case InstructionType::ADD:
        {
            uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
            uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
            variables[instr.args[0]] = val2 + val3;
            oss << "ADD: " << instr.args[0] << " = " << val2 << " + " << val3 << " -> " << variables[instr.args[0]];
            
            
            
            break;
        }
        case InstructionType::SUBTRACT:
        {
            uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
            uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
            uint16_t result = (val2 > val3) ? val2 - val3 : 0; 
            variables[instr.args[0]] = result;
            oss << "SUBTRACT: " << instr.args[0] << " = " << val2 << " - " << val3 << " -> " << result;
            
            
            
            break;
        }
        case InstructionType::SLEEP:
        {
            uint8_t sleepTicks = static_cast<uint8_t>(std::stoul(instr.args[0]));
            sleepUntilCycle = global_simulated_cycles + sleepTicks;
            oss << "Sleeping for " << std::to_string(sleepTicks) << " cycles.";
            break;
        }
        case InstructionType::MULTIPLY:
        {
            uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
            uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
            variables[instr.args[0]] = val2 * val3;
            oss << "MULTIPLY: " << instr.args[0] << " = " << val2 << " * " << val3 << " -> " << variables[instr.args[0]];
            
            
            

            break;
        }
        case InstructionType::DIVIDE:
        {
            uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
            uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
            uint16_t result = (val3 == 0) ? 0 : (val2 / val3);
            variables[instr.args[0]] = result;
            oss << "DIVIDE: " << instr.args[0] << " = " << val2 << " / " << val3 << " -> " << result;
            
            
            

            break;
        }
        case InstructionType::FOR:
            
            
            for (const auto &nested_instr : instr.nestedInstructions)
            {
                
                logs.push_back("Executing nested instruction inside FOR loop.");
            }
            oss << "Executed a FOR loop.";
            break;
        case InstructionType::WRITE:
        {
            if (instr.args.size() < 2)
            {
                oss << "WRITE: Invalid arguments.";
                break;
            }

            std::string hexAddrStr = instr.args[0];
            std::string valueStr = instr.args[1];
            uint32_t virtualAddress;
            uint16_t value;

            try
            {
                virtualAddress = std::stoul(hexAddrStr, nullptr, 16);
                value = variables.count(valueStr) ? variables[valueStr] : static_cast<uint16_t>(std::stoul(valueStr));
            }
            catch (const std::exception &e)
            {
                oss << "WRITE ERROR: Invalid address or value format.";
                break;
            }

            if (!isValidMemoryAddress(virtualAddress) || virtualAddress + 1 >= pageTable.size() * memPerFrame)
            {
                recordMemoryViolation(virtualAddress);
                oss << "MEMORY ACCESS VIOLATION: Invalid address " << hexAddrStr;
                return false;
            }

            int vpn = virtualAddress / memPerFrame;
            int offset = virtualAddress % memPerFrame;

            
            accessMemory(this, vpn, global_simulated_cycles);

            
            int frameIdx = pageTable[vpn].frameIndex;

            
            frameTable[frameIdx].data[offset] = static_cast<uint8_t>(value & 0xFF);            
            frameTable[frameIdx].data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF); 

            
            pageTable[vpn].dirty = true;

            oss << "WRITE: memory[" << hexAddrStr << "] = " << value;
            break;
        }
            

        case InstructionType::READ:
        {
            if (instr.args.size() < 2)
            {
                oss << "READ: Invalid arguments.";
                break;
            }

            std::string varName = instr.args[0];
            std::string hexAddrStr = instr.args[1];
            uint32_t virtualAddress;

            try
            {
                virtualAddress = std::stoul(hexAddrStr, nullptr, 16);
            }
            catch (const std::exception &e)
            {
                oss << "READ ERROR: Invalid address format.";
                break;
            }

            if (!isValidMemoryAddress(virtualAddress) || virtualAddress + 1 >= pageTable.size() * memPerFrame)
            {
                recordMemoryViolation(virtualAddress);
                oss << "MEMORY ACCESS VIOLATION: Invalid address " << hexAddrStr;
                return false;
            }

            if (variables.size() >= 32 && variables.find(varName) == variables.end())
            {
                oss << "Symbol table full. Cannot declare " << varName;
                break;
            }

            int vpn = virtualAddress / memPerFrame;
            int offset = virtualAddress % memPerFrame;

            
            accessMemory(this, vpn, global_simulated_cycles);

            
            int frameIdx = pageTable[vpn].frameIndex;

            
            uint8_t lowByte = frameTable[frameIdx].data[offset];
            uint8_t highByte = frameTable[frameIdx].data[offset + 1];
            uint16_t value = (highByte << 8) | lowByte;

            variables[varName] = value;

            oss << "READ: " << varName << " = memory[" << hexAddrStr << "] -> " << value;
            break;
        }
        }

        
        std::time_t timestamp;                                                  
        std::time(&timestamp);                                                  
        char buffer[30];                                                        
        std::tm *timeinfo = std::localtime(&timestamp);                         
        std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo); 

        

        std::ostringstream log_entry;
        log_entry << "(" << buffer << ") Core:" << coreId << " " << oss.str();
        logs.push_back(log_entry.str());

        currentInstruction++;
        remainingInstructions = instructions.size() - currentInstruction;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        return true;
    }

    bool canExecute() const
    {
        return !hasFinished();
    }

    
    int getRemainingInstructions() const { return remainingInstructions; }
    int getCurrentInstruction() const { return currentInstruction; }
    int getTotalInstructions() const { return totalInstructions; }
    bool hasFinished() const { return remainingInstructions == 0; }
    std::string getName() const { return name; }
    int getId() const { return id; }
    std::string getTimeCreated() const { return timeCreated; }
    int getAssignedCore() const { return assignedCore; }

    void setAssignedCore(int core) { assignedCore = core; }
    const std::vector<std::string> &getLogs() const { return logs; }

    uint16_t getRequiredMemorySize() const { return requiredMemorySize; }

    void setInstructions(const std::vector<Instruction> &userInstructions)
    {
        instructions = userInstructions;
        totalInstructions = instructions.size();
        remainingInstructions = totalInstructions;
    }
};

void printVMStat()
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    int totalMem = maxOverallMem;                                     
    int usedMem = (totalFrames - freeFrameList.size()) * memPerFrame; 
    int freeMem = totalMem - usedMem;
    uint64_t currentIdleTicks = totalIdleTicks.load();
    uint64_t currentActiveTicks = totalActiveTicks.load();
    uint64_t totalTicks = currentIdleTicks + currentActiveTicks;

    std::cout << "\n=== VMSTAT ===\n";
    std::cout << "memory\n";
    std::cout << "Total memory      : " << totalMem << " bytes\n";
    std::cout << "Used memory       : " << usedMem << " bytes\n";
    std::cout << "Free memory       : " << freeMem << " bytes\n";
    std::cout << "cpu\n";
    std::cout << "Idle cpu ticks    : " << currentIdleTicks << "\n";
    std::cout << "Active cpu ticks  : " << currentActiveTicks << "\n";
    std::cout << "Total cpu ticks   : " << totalTicks << "\n";
    std::cout << "paging\n";
    std::cout << "Num paged in      : " << numPagedIn.load() << "\n";
    std::cout << "Num paged out     : " << numPagedOut.load() << "\n";
    std::cout << "=================\n";
}


Process *getProcessById(int pid)
{
    
    std::lock_guard<std::mutex> lock(processMutex);
    for (auto &p_ptr : allProcesses)
    {
        if (p_ptr->getId() == pid)
        {
            return p_ptr.get();
        }
    }
    return nullptr; 
}


int findVictimFrame_LRU()
{
    uint64_t oldestTimestamp = ULLONG_MAX;
    int victimFrameIndex = -1;

    for (int i = 0; i < frameTable.size(); ++i)
    {
        if (frameTable[i].occupied && frameTable[i].lastUsedTimestamp < oldestTimestamp)
        {
            oldestTimestamp = frameTable[i].lastUsedTimestamp;
            victimFrameIndex = i;
        }
    }
    return victimFrameIndex;
}


void handlePageFault(Process *proc, int vpn, unsigned long long curCycle)
{
    std::lock_guard<std::mutex> lock(memoryMutex); 

    int frameToUse = -1;

    if (!freeFrameList.empty())
    {
        
        frameToUse = freeFrameList.front();
        freeFrameList.pop_front();
    }
    else
    {
        
        frameToUse = findVictimFrame_LRU();
        if (frameToUse == -1)
        {
            std::cerr << "CRITICAL: No victim frame could be found!" << std::endl;
            return;
        }

        
        FrameEntry &victimFrame = frameTable[frameToUse];
        Process *victimProcess = getProcessById(victimFrame.processId);

        if (victimProcess)
        {
            Process::PageTableEntry &victimPageEntry = victimProcess->pageTable[victimFrame.virtualPageNum];
            if (victimPageEntry.dirty)
            {
                
                writePageToBackingStore(victimProcess->getId(), victimFrame.virtualPageNum, victimFrame.data);
                numPagedOut++;
            }
            
            victimPageEntry.present = false;
            victimPageEntry.frameIndex = -1;
        }
    }

    
    
    std::vector<uint8_t> pageData = readPageFromBackingStore(proc->getId(), vpn);

    
    numPagedIn++;
    frameTable[frameToUse].occupied = true;
    frameTable[frameToUse].processId = proc->getId();
    frameTable[frameToUse].virtualPageNum = vpn;
    frameTable[frameToUse].lastUsedTimestamp = curCycle;
    frameTable[frameToUse].data = pageData; 

    
    Process::PageTableEntry &newPageEntry = proc->pageTable[vpn];
    newPageEntry.present = true;
    newPageEntry.frameIndex = frameToUse;
    newPageEntry.dirty = false; 
}

bool accessMemory(Process *proc, int vpn, int curCycle)
{
    if (vpn >= proc->pageTable.size())
    {
        
        
        return false;
    }

    Process::PageTableEntry &pageEntry = proc->pageTable[vpn];

    if (pageEntry.present)
    {
        
        std::lock_guard<std::mutex> lock(memoryMutex);
        frameTable[pageEntry.frameIndex].lastUsedTimestamp = curCycle;
        return true;
    }
    else
    {
        
        handlePageFault(proc, vpn, curCycle);
        return true; 
    }
}

void cpuCycleLoop()
{
    while (schedulerRunning)
    {
        global_simulated_cycles++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (global_simulated_cycles % 100 == 0)
        {
            std::this_thread::yield();
        }
    }
}

class Screen
{
public:
    std::string screenName;
    int curInstruction;
    int totalInstruction;
    std::string timeCreated;
    bool isDetached;

    Screen() : isDetached(false) {}
};

std::vector<Screen> screenList;


void setColor(int color)
{
    switch (color)
    {
    case 7:
        std::cout << "\033[37m";
        break; 
    case 10:
        std::cout << "\033[32m";
        break; 
    case 14:
        std::cout << "\033[33m";
        break; 
    default:
        std::cout << "\033[37m";
        break; 
    }
}

void printASCII(std::string fileName)
{
    std::string line = "";
    std::ifstream inFile;
    inFile.open(fileName);
    if (inFile.is_open())
    {
        while (std::getline(inFile, line))
        {
            std::cout << line << std::endl;
        }
    }
    else
    {
        std::cout << "File failed to load. " << std::endl;
    }
    inFile.close();
}


void intro()
{
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


class Scheduler
{
public:
    virtual ~Scheduler() = default;
    virtual void addProcess(Process *process) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};



class FCFSScheduler : public Scheduler
{
private:
    int numCores; 
    std::vector<std::thread> coreThreads;
    std::queue<Process *> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};

public:
    FCFSScheduler(int cores) : numCores(cores) {}

    ~FCFSScheduler()
    {
        stop();
    }

    void addProcess(Process *process) override
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        processQueue.push(process);
        queueCV.notify_all();
    }

    void start() override
    {
        running = true;
        schedulerRunning = true;

        
        for (int i = 0; i < numCores; i++)
        {
            coreThreads.emplace_back([this, i]()
                                     { this->coreWorker(i); });
        }

        std::cout << "Scheduler started with " << numCores << " cores." << std::endl
                  << std::endl;
    }

    void stop() override
    {

        running = false;
        schedulerRunning = false;
        queueCV.notify_all();

        for (auto &thread : coreThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        coreThreads.clear();
    }

    void coreWorker(int coreId)
    {
        while (running)
        {
            Process *processToExecute = nullptr;

            
            {
                std::unique_lock<std::mutex> lock(queueMutex);

                
                if (processQueue.empty())
                {
                    
                    queueCV.wait_for(lock, std::chrono::milliseconds(1));
                    totalIdleTicks++; 

                    
                    if (processQueue.empty() || !running)
                    {
                        continue;
                    }
                }

                
                processToExecute = processQueue.front();
                processQueue.pop();
            }

            
            {
                std::lock_guard<std::mutex> pLock(processMutex);
                runningProcesses.push_back(processToExecute);
            }

            processToExecute->setAssignedCore(coreId);
            while (processToExecute->canExecute() && running)
            {
                processToExecute->executeInstruction(coreId);
                totalActiveTicks++;
            }
            
            {
                std::lock_guard<std::mutex> pLock(processMutex);
                runningProcesses.erase(
                    std::remove_if(runningProcesses.begin(), runningProcesses.end(),
                                   [processToExecute](const Process *p)
                                   { return p->getId() == processToExecute->getId(); }),
                    runningProcesses.end());

                finishedProcesses.push_back(processToExecute);
            }
        }
    }

    bool isRunning() const override { return running; }
};



class RoundRobinScheduler : public Scheduler
{
private:
    int numCores;
    int quantum;
    std::vector<std::thread> coreThreads;
    std::queue<Process *> processQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};
    std::set<int> activeProcessIds; 

public:
    RoundRobinScheduler(int cores, int quantumCycles) : numCores(cores), quantum(quantumCycles) {}

    ~RoundRobinScheduler()
    {
        stop();
    }

    void addProcess(Process *process) override
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            processQueue.push(process);
        }
        queueCV.notify_one();
    }

    void start() override
    {
        running = true;
        schedulerRunning = true;

        for (int i = 0; i < numCores; ++i)
        {
            coreThreads.emplace_back([this, i]()
                                     { this->coreWorker(i); });
        }
        std::cout << "Round Robin Scheduler started with " << numCores << " cores and quantum of " << quantum << " cycles." << std::endl
                  << std::endl;
    }

    void stop() override
    {
        running = false;
        schedulerRunning = false;
        queueCV.notify_all();

        for (auto &thread : coreThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        coreThreads.clear();
    }

    void coreWorker(int coreId)
{
    while (running)
    {
        Process *processToExecute = nullptr;
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (processQueue.empty())
            {
                queueCV.wait_for(lock, std::chrono::milliseconds(1));
                totalIdleTicks++;
                if (processQueue.empty() || !running)
                {
                    continue;
                }
            }

            processToExecute = processQueue.front();
            processQueue.pop();

            
            if (!processQueue.empty()) { 
                size_t checked_count = 0;
                size_t q_size = processQueue.size() + 1; 

                while (activeProcessIds.count(processToExecute->getId()))
                {
                    if (checked_count >= q_size) {
                        
                        processToExecute = nullptr;
                        break;
                    }
                    
                    processQueue.push(processToExecute);
                    processToExecute = processQueue.front();
                    processQueue.pop();
                    checked_count++;
                }
            }
            
            
            if (processToExecute != nullptr && !activeProcessIds.count(processToExecute->getId()))
            {
                activeProcessIds.insert(processToExecute->getId());
            } else {
                
                if(processToExecute != nullptr) processQueue.push(processToExecute);
                processToExecute = nullptr;
            }
        } 

        if (processToExecute == nullptr)
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> pLock(processMutex);
            if (std::find(runningProcesses.begin(), runningProcesses.end(), processToExecute) == runningProcesses.end())
            {
                runningProcesses.push_back(processToExecute);
            }
        }

        processToExecute->setAssignedCore(coreId);

        int executedCycles = 0;
        while (executedCycles < quantum && processToExecute->canExecute() && running)
        {
            totalActiveTicks++;
            processToExecute->executeInstruction(coreId);
            executedCycles++;

            if (executedCycles % 10 == 0)
            {
                std::this_thread::yield();
            }
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            activeProcessIds.erase(processToExecute->getId());
        }

        if (processToExecute->hasFinished())
        {
            std::lock_guard<std::mutex> pLock(processMutex);
            runningProcesses.erase(std::remove(runningProcesses.begin(), runningProcesses.end(), processToExecute), runningProcesses.end());
            finishedProcesses.push_back(processToExecute);
        }
        else if (running)
        {
            addProcess(processToExecute);
        }
    }
}
    bool isRunning() const override { return running; }
};

void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void initialize()
{
    if (readConfig() == true)
    {
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

        frameTable.resize(totalFrames);
        for (int i = 0; i < totalFrames; ++i)
        {
            freeFrameList.push_back(i);
            if (memPerFrame > 0)
                frameTable[i].data.resize(memPerFrame, 0);
        }
    }

    else
        return;
}

Screen curScreen;

void createScreen(std::string &screenName, uint16_t memorySize)
{
    std::time_t timestamp;
    std::time(&timestamp);

    Screen newScreen;
    newScreen.screenName = screenName;
    int instructionCount = minIns + (std::rand() % (maxIns - minIns + 1));
    newScreen.totalInstruction = instructionCount;
    newScreen.curInstruction = 0;

    char buffer[30];
    std::tm *timeinfo = std::localtime(&timestamp);
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", timeinfo);
    newScreen.timeCreated = buffer;

    newScreen.isDetached = false;

    screenList.emplace_back(newScreen);
    curScreen = newScreen;
    
    auto newProcess = std::make_unique<Process>(screenName, instructionCount, memorySize);
    allProcesses.push_back(std::move(newProcess));
}

void manualProcessesScheduler()
{
    if (scheduler && scheduler->isRunning())
    {
        return;
    }

    if (scheduler == nullptr)
    {
        if (schedulerType == "fcfs")
        {
            scheduler = new FCFSScheduler(numCPU);
        }
        else if (schedulerType == "rr")
        {
            scheduler = new RoundRobinScheduler(numCPU, quantumCycles);
        }
        else
        {
            std::cout << "Error: Unknown scheduler type '" << schedulerType << "' in config.txt. Aborting." << std::endl;
            return;
        }
    }
    {
        std::lock_guard<std::mutex> pLock(processMutex);
        for (const auto &process : allProcesses)
        {
            if (!process->hasFinished())
            {
                scheduler->addProcess(process.get());
            }
        }
    }
    schedulerRunning = true;
    std::thread(cpuCycleLoop).detach();
    scheduler->start();
}


void processSmi()
{
    std::lock_guard<std::mutex> lock(processMutex);

    std::set<int> usedCores;
    for (const auto &process : runningProcesses)
    {
        if (process->getAssignedCore() != -1)
        {
            usedCores.insert(process->getAssignedCore());
        }
    }

    int coresUsed = usedCores.size();
    double cpuUtilization = (numCPU > 0) ? (static_cast<double>(coresUsed) / numCPU * 100.0) : 0.0;

    uint64_t totalMemoryUsed = 0;
    for (const auto &process : runningProcesses)
    {
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
    for (const auto &process : runningProcesses)
    {
        setColor(7);
        std::cout << process->getName() << " ";
        setColor(14);
        std::cout << process->getRequiredMemorySize() << "B\n";
    }
    setColor(7);
    std::cout << "----------------------------------------------";
}

void screenLS()
{
    std::lock_guard<std::mutex> lock(processMutex);

    std::set<int> usedCores;
    for (const auto &process : runningProcesses)
    {
        if (process->getAssignedCore() != -1)
        {
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

    for (const auto &process : runningProcesses)
    {
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

    for (const auto &process : finishedProcesses)
    {
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

void screen(std::string &screenCommand)
{
    std::istringstream iss(screenCommand);
    std::string command, option, argument, memStr;
    iss >> command >> option >> argument;

    if (option == "-s" && !argument.empty())
    {
        iss >> memStr;

        if (argument.empty() || memStr.empty())
        {
            std::cout << "Usage: screen -s <process_name> <process_memory_size>" << std::endl;
            return;
        }

        uint16_t memSize;
        try
        {
            memSize = std::stoul(memStr);
        }
        catch (...)
        {
            std::cout << "Invalid memory size format." << std::endl;
            return;
        }

        if (!isValidMemorySize(memSize))
        {
            std::cout << "invalid memory allocation" << std::endl;
            return;
        }

        createScreen(argument, memSize);
        Process *newProcess = allProcesses.back().get();

        if (scheduler && scheduler->isRunning())
        {
            scheduler->addProcess(newProcess);
        }
        else
        {
            manualProcessesScheduler();
        }

        
        std::cout << "Screen created: " << newProcess->getName()
                  << " with " << newProcess->getTotalInstructions()
                  << " instructions." << std::endl;
        return;
    }
    else if (option == "-r" && !argument.empty())
    {
        std::string screenName = argument;
        
        
        auto finishedIt = std::find_if(finishedProcesses.begin(), finishedProcesses.end(),
                                       [&screenName](const Process *p) { return p->getName() == screenName; });

        if (finishedIt != finishedProcesses.end())
        {
            Process *finishedProcess = *finishedIt;
            if (finishedProcess->hasMemoryViolation())
            {
                std::cout << "Process \"" << screenName
                          << "\" shut down due to memory access violation error that occurred at "
                          << finishedProcess->getViolationTimestamp()
                          << ". 0x" << std::hex << std::uppercase << finishedProcess->getViolationAddress()
                          << std::dec << " invalid." << std::endl;
            }
            else
            {
                std::cout << "Process \"" << screenName
                          << "\" has already finished execution." << std::endl;
            }
            return; 
        }

        
        auto it = std::find_if(allProcesses.begin(), allProcesses.end(),
                               [&screenName](const std::unique_ptr<Process> &p) { return p->getName() == screenName; });

        if (it != allProcesses.end())
        {
            Process* process = it->get();
            std::cout << "\n--- Status for screen: " << process->getName() << " ---" << std::endl;
            std::cout << "  ID: " << process->getId() << std::endl;
            std::cout << "  Time Created: " << process->getTimeCreated() << std::endl;
            std::cout << "  Assigned Core: " << (process->getAssignedCore() == -1 ? "Waiting" : std::to_string(process->getAssignedCore())) << std::endl;
            std::cout << "  Progress: " << process->getCurrentInstruction()
                      << " / " << process->getTotalInstructions() << " instructions." << std::endl;
            std::cout << "  Memory Required: " << process->getRequiredMemorySize() << " bytes" << std::endl;
            std::cout << "--- End of Status ---" << std::endl;
        }
        else
        {
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl;
        }
        
        
        return;
    }
    else if (option == "-c" && !argument.empty())
    {
        std::string processName = argument;
        std::string memStr;
        iss >> memStr;

        if (memStr.empty())
        {
            std::cout << "invalid command" << std::endl;
            return;
        }

        uint16_t memSize;
        try
        {
            memSize = static_cast<uint16_t>(std::stoi(memStr));
        }
        catch (...)
        {
            std::cout << "invalid command" << std::endl;
            return;
        }

        if (!isValidMemorySize(memSize))
        {
            std::cout << "invalid command" << std::endl;
            return;
        }

        std::string instructionsString;
        std::getline(iss >> std::ws, instructionsString);

        if (instructionsString.length() >= 2 && instructionsString.front() == '"' && instructionsString.back() == '"')
        {
            instructionsString = instructionsString.substr(1, instructionsString.length() - 2);
        }
        else
        {
            std::cout << "invalid command: instructions must be enclosed in double quotes" << std::endl;
            return;
        }

        if (instructionsString.empty())
        {
            std::cout << "invalid command" << std::endl;
            return;
        }

        std::vector<Instruction> parsed = parseUserInstructions(instructionsString);
        if (parsed.size() < 1 || parsed.size() > 50)
        {
            std::cout << "invalid command" << std::endl;
            return;
        }

        createScreen(processName, memSize);
        Process *newProcess = allProcesses.back().get();
        newProcess->setInstructions(parsed);

        if (scheduler && scheduler->isRunning())
        {
            scheduler->addProcess(newProcess);
        }
        else
        {
            manualProcessesScheduler();
        }
        
        
        std::cout << "Screen created: " << newProcess->getName()
                  << " with " << newProcess->getTotalInstructions()
                  << " instructions." << std::endl;
        return;
    }
    else if (option == "-d" && !argument.empty())
    {
        std::string screenName = argument;
        bool found = false;
        for (auto &scr : screenList)
        {
            if (scr.screenName == screenName)
            {
                scr.isDetached = true;
                std::cout << "Screen \"" << screenName << "\" detached." << std::endl;
                found = true;
                break;
            }
        }
        if (!found)
        {
            std::cout << "Screen \"" << screenName << "\" not found." << std::endl;
        }
    }
    else if (option == "-ls")
    {
        screenLS();
        return;
    }
    else {
        std::cout << "Invalid 'screen' command option." << std::endl;
    }
}
void processCreationLoop()
{
    static int i = 0;
    long long lastCycle = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (processCreationRunning)
    {
        if (scheduler && scheduler->isRunning())
        {
            int coresInUse = 0;
            {
                std::lock_guard<std::mutex> lock(processMutex);
                std::set<int> usedCores;
                for (const auto &process : runningProcesses)
                {
                    if (process->getAssignedCore() != -1)
                    {
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
            if (coresInUse < numCPU && (global_simulated_cycles - lastCycle >= batchProcessFreq))
            {
                std::string processName = "process_";
                if (i < 10)
                {
                    processName += "0";
                }
                processName += std::to_string(i++);

                uint16_t memForNewProcess = getMemorySize();
                createScreen(processName, memForNewProcess);
                scheduler->addProcess(allProcesses.back().get());

                lastCycle = global_simulated_cycles;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void schedulerStart()
{
    if (scheduler == nullptr)
    {
        if (schedulerType == "fcfs")
        {
            scheduler = new FCFSScheduler(numCPU);
        }
        else if (schedulerType == "rr")
        {
            scheduler = new RoundRobinScheduler(numCPU, quantumCycles);
        }
        else
        {
            std::cout << "Error: Unknown scheduler type '" << schedulerType << "' in config.txt. Aborting." << std::endl;
            return;
        }
    }

    if (!scheduler->isRunning())
    {

        {
            std::lock_guard<std::mutex> pLock(processMutex);
            for (const auto &process : allProcesses)
            {
                if (!process->hasFinished())
                {
                    scheduler->addProcess(process.get());
                }
            }
        }

        std::cout << "Creating processes. Enter \"scheduler-stop\" to cease." << std::endl;
        processCreationRunning = true;
        schedulerRunning = true;
        std::thread(cpuCycleLoop).detach();        
        std::thread(processCreationLoop).detach(); 
        scheduler->start();
    }
    else
    {
        std::cout << "Scheduler is already running." << std::endl
                  << std::endl;
    }
}


void schedulerStop()
{
    std::cout << "Stopping new process creation..." << std::endl;
    processCreationRunning = false; 

    if (scheduler != nullptr && scheduler->isRunning())
    {
        std::cout << "Waiting for running processes to complete their tasks..." << std::endl;
        scheduler->stop(); 
        schedulerRunning = false; 
        std::cout << "\nScheduler has stopped." << std::endl;
    }
    else
    {
        std::cout << "Scheduler was not running." << std::endl;
    }

    std::cout << "Total screens/processes created: " << screenList.size() << std::endl;
}
void reportUtil()
{
    std::ofstream outFile("csopesy-log.txt");
    if (!outFile.is_open())
    {
        std::cerr << "Failed to open file for writing.\n";
        return;
    }

    std::lock_guard<std::mutex> lock(processMutex);

    std::set<int> usedCores;
    for (const auto &process : runningProcesses)
    {
        if (process->getAssignedCore() != -1)
        {
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

    for (const auto &process : runningProcesses)
    {
        outFile << process->getName() << " (" << process->getTimeCreated() << ")  "
                << "Core: " << process->getAssignedCore() << "  "
                << process->getCurrentInstruction() << "  /  " << process->getTotalInstructions() << "\n";
    }

    outFile << "\n\n------------------\n\n";

    outFile << "\nFinished processes:\n";
    for (const auto &process : finishedProcesses)
    {
        outFile << process->getName() << "  (" << process->getTimeCreated() << ")  "
                << "Finished " << process->getTotalInstructions() << "  /  " << process->getTotalInstructions() << "\n";
    }

    outFile << "================\n\n";

    outFile.close();
    std::cout << "Report generated at opesy-log.txt!\n\n";
}

void commandProcessor() {
    std::string command;
    intro();

    while (systemRunning)
    {
        
        

        setColor(7);
        std::cout << "\nroot:\\> ";
        std::getline(std::cin >> std::ws, command);

        
        if (!systemRunning) break;

        if (command == "exit")
        {
            std::cout << "Shutting down scheduler and processes..." << std::endl;
            if (scheduler != nullptr && scheduler->isRunning())
            {
                schedulerStop(); 
            }
            systemRunning = false; 
            std::cout << "Thank you for using the program." << std::endl;
            
            break;
        }
        else if (command == "clear")
        {
            clearScreen();
            intro();
        }
        else if (command == "initialize")
        {
            initialize();
        }
        else if (!initialized)
        {
            std::cout << "Command is not recognized. Please initialize the system first by using the 'initialize' command.\n\n";
        }
        else if (command.rfind("screen", 0) == 0)
        {
            screen(command);
        }
        else if (command == "scheduler-start")
        {
            schedulerStart();
        }
        else if (command == "scheduler-stop")
        {
            schedulerStop();
        }
        else if (command == "report-util")
        {
            reportUtil();
        }
        else if (command == "process-smi")
        {
            processSmi();
        }
        else if (command == "vmstat")
        {
            printVMStat();
        }
        else
        {
            
            if (!command.empty()) {
                std::cout << "Unknown command. Please try again.\n\n";
            }
        }
    }
}

void menu()
{
    
    std::thread commandThread(commandProcessor);

    
    while (systemRunning) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    
    
    commandThread.join();
}


int main()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    menu(); 
    
    
    if (scheduler) {
        delete scheduler;
        scheduler = nullptr;
    }
    
    
    return 0;
}