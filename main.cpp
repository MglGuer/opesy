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
std::vector<bool> memoryBlock; //true = used, false = free

//FOR CONFIG.txt
int numCPU; //number of cores (between 1-128 inclusive)
std::string schedulerType; //scheduler type("fcfs" or "rr")
uint64_t quantumCycles; //for round robin, how many ticks before swapping (1-2^32 inclusive)
uint64_t batchProcessFreq; //1 process every x cycles (1-2^32 inclusive)
uint64_t minIns;
uint64_t maxIns;
uint64_t delaysPerExec; //1 instruction every x cycles (0 - 2^32 inclusive) if 0, it executes every cycle
uint64_t maxOverallMem;
uint64_t memPerFrame;
uint64_t memPerProc;
uint64_t totalFrames = 0;
bool initialized = false;

//Instruction Types
enum class InstructionType {
    PRINT,
    ADD,
    DECLARE,
    SUBTRACT,
    SLEEP,
    FOR
};

struct Instruction {
    InstructionType type;
    std::vector<std::string> args;
    std::vector<Instruction> nestedInstructions;
};

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
    bool outOfRangeMemPerProc = false;
    
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
            if (quantumCycles < 1 || quantumCycles > 4294967296){
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
        else if (key == "mem-per-proc"){
            iss >> memPerProc;
            if (memPerProc < 1 || memPerProc > 4294967296){
                outOfRangeMemPerProc = true;
                std::cout << "Error: mem-per-proc must be between 1 and 4294967296 (inclusive). Please reconfigure config.txt." << std::endl;
            }
        }

    }
    if (outOfRangeCPU || outOfRangeScheduler || outOfRangeQuantum || outOfRangeBatch || outOfRangeMin || outOfRangeMax || outOfRangeDelay)
        return false;
    totalFrames = maxOverallMem / memPerFrame;
    memoryBlock = std::vector<bool>(totalFrames, false); // Initialize memory usage tracking
    return true;
}

//NEW
int allocateMemory (int framesNeeded){
    std::lock_guard<std::mutex> lock(memoryMutex);
    int start = -1;
    int count = 0;

    for (int i=0; i<totalFrames; ++i) {
        if (!memoryBlock[i]) {
            if (count == 0)
                start = i;
            count++;
            if (count == framesNeeded) {
                // Allocate frames
                for (int j = start; j < start + framesNeeded; ++j) {
                    memoryBlock[j] = true;
                }
                return start; // Return the starting index of allocated frames
            }
        }
        else{
            count = 0; // Reset count if a used frame is encountered
        }
    }
    return -1; // Not enough memory available
}

//NEW
void freeMemory(int startIndex, int frames) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    for (int i = startIndex; i < startIndex + frames; ++i)
        memoryBlock[i] = false;
}

//NEW
int countExternalFragmentation(int framesNeeded) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    int fragmentation = 0;
    int currentBlock = 0;

    for (bool bit : memoryBlock) {
        if (!bit) {
            currentBlock++;
        } else {
            if (currentBlock > 0 && currentBlock < framesNeeded) {
                fragmentation += currentBlock;
            }
            currentBlock = 0;
        }
    }

    // Check trailing block
    if (currentBlock > 0 && currentBlock < framesNeeded) {
        fragmentation += currentBlock;
    }

    return (fragmentation * memPerFrame) / 1024; // Return in KB
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

public:
    // Constructor
    Process(const std::string& processName, int numInstructions = 100) 
        : name(processName), totalInstructions(numInstructions), 
          remainingInstructions(numInstructions), currentInstruction(0), assignedCore(-1) {
        
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
          variables(std::move(other.variables)), sleepUntilCycle(other.sleepUntilCycle.load()) {
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
                case InstructionType::DECLARE: {
                    variables[instr.args[0]] = static_cast<uint16_t>(std::stoul(instr.args[1]));
                    oss << "Declared " << instr.args[0] << " = " << instr.args[1];
                    break;
                }
                case InstructionType::ADD: {
                    uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
                    uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
                    variables[instr.args[0]] = val2 + val3;
                    oss << "ADD: " << instr.args[0] << " = " << val2 << " + " << val3 << " -> " << variables[instr.args[0]];
                    break;
                }
                case InstructionType::SUBTRACT: {
                    uint16_t val2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : 0;
                    uint16_t val3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : static_cast<uint16_t>(std::stoul(instr.args[2]));
                    uint16_t result = (val2 > val3) ? val2 - val3 : 0; // Clamp at 0
                    variables[instr.args[0]] = result;
                    oss << "SUBTRACT: " << instr.args[0] << " = " << val2 << " - " << val3 << " -> " << result;
                    break;
                }
                case InstructionType::SLEEP: {
                    uint8_t sleepTicks = static_cast<uint8_t>(std::stoul(instr.args[0]));
                    sleepUntilCycle = global_simulated_cycles + sleepTicks;
                    oss << "Sleeping for " << std::to_string(sleepTicks) << " cycles.";
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
};

// NEW: Function to dump memory status to a file
void dumpMemoryStatus(uint64_t curCycle) {
    std::string filename = "memory_stamp_" + std::to_string(curCycle) + ".txt";
    std::ofstream outFile(filename);
    if (!outFile.is_open()) return;

    // Timestamp
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %I:%M:%S%p", &tm);

    // Count processes in memory
    int procInMem = 0;
    {
        std::lock_guard<std::mutex> lock(processMutex);
        for (const auto& p : runningProcesses)
            if (p->getMemoryStartIndex() != -1) procInMem++;
    }

    int fragKB = countExternalFragmentation(memPerProc / memPerFrame);

    outFile << "Timestamp: (" << timestamp << ")\n";
    outFile << "Number of processes in memory: " << procInMem << "\n";
    outFile << "Total external fragmentation in KB: " << fragKB << "\n\n";

    outFile << "----end---- = " << maxOverallMem << "\n";

    // Collect process layout
    struct MemoryEntry {
        int lower;
        int upper;
        std::string name;
    };

    std::vector<MemoryEntry> memLayout;

    {
        std::lock_guard<std::mutex> lock(processMutex);
        for (const auto& p : runningProcesses) {
            if (p->getMemoryStartIndex() != -1) {
                int start = p->getMemoryStartIndex() * memPerFrame;
                int end = start + (p->getFramesAllocated() * memPerFrame);
                memLayout.push_back({start, end, p->getName()});
            }
        }
    }

    // Sort from top to bottom (descending upper address)
    std::sort(memLayout.begin(), memLayout.end(), [](const MemoryEntry& a, const MemoryEntry& b) {
        return a.upper > b.upper;
    });

    for (const auto& entry : memLayout) {
        outFile << entry.upper << "\n";
        outFile << entry.name << "\n";
        outFile << entry.lower << "\n\n";
    }

    outFile << "----start---- = 0\n";

    outFile.close();
}


void cpuCycleLoop() {
    while (schedulerRunning) {
        global_simulated_cycles++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(global_simulated_cycles % quantumCycles == 0){
            dumpMemoryStatus(global_simulated_cycles);
        }
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
    std::cout << "28/06/2025" << std::endl;
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

            // Get a process from the queue
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCV.wait(lock, [this]() { return !processQueue.empty() || !running; });
            
                if (!running) return;

                processToExecute = processQueue.front();
                processQueue.pop();

                {
                    std::lock_guard<std::mutex> pLock(processMutex);
                    runningProcesses.push_back(processToExecute);
                }
            }

        // Assign process to this core and execute until it's done
            processToExecute->setAssignedCore(coreId);
            while (processToExecute->canExecute() && running) {
                processToExecute->executeInstruction(coreId);
            }

            // Move to finished
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
                if(processToExecute -> getMemoryStartIndex() == -1){
                    int framesNeeded = memPerProc / memPerFrame;
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
        std::cout << "Memory per process: " << memPerProc << std::endl;
        std::cout << "Total frames: " << totalFrames << std::endl;
        std::cout << "-------------------------------------------------------------------------" << std::endl;
        std::cout << "System Initialized. You may now create screens and perform other actions.\n\n";
    }
    else
        return;
    
}

Screen curScreen;

void createScreen(std::string &screenName) {
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
    auto newProcess = std::make_unique<Process>(screenName, instructionCount);
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
    std::string command, option, argument;
    iss >> command >> option >> argument;

    if(option == "-s" && !argument.empty()) {
        //std::vector<Process*> justCreated; // A temporary list to hold newly created processes

        createScreen(argument);
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

            if (coresInUse < numCPU && (global_simulated_cycles - lastCycle >= batchProcessFreq)) {
                std::string processName = "process_";
                if (i < 10) {
                    processName += "0";
                }
                processName += std::to_string(i++);

                createScreen(processName);
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

void schedulerStop() {
    std::cout << "Stopping scheduler and process creation..." << std::endl;
    
    processCreationRunning = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (scheduler != nullptr && scheduler->isRunning()) {
        scheduler->stop();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        {
            std::lock_guard<std::mutex> pLock(processMutex);
            
            // Move any remaining running processes to finished
            for (Process* proc : runningProcesses) {
                if (proc->hasFinished()) {
                    finishedProcesses.push_back(proc);
                } else {
                    // Mark incomplete processes as finished for clean shutdown
                    finishedProcesses.push_back(proc);
                }
            }
            
            runningProcesses.clear();
        }
        
        std::cout << "Total screens/processes created: " << screenList.size() << std::endl;
    } else {
        std::cout << "Scheduler is not running." << std::endl;
    }
    
    schedulerRunning = false;
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
    intro();
    while (true) {
        
        setColor(7);
        std::cout << "\nroot:\\> ";
        std::string command;
        std::getline(std::cin >> std::ws, command);

        if(command == "exit") {
            if (scheduler != nullptr) {
                scheduler->stop();
                delete scheduler;
            }
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