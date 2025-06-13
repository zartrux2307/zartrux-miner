#include "JobManager.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iostream>
#include <cstring>
#include "utils/Logger.h"

JobManager::JobManager()
    : m_iaEndpoint("")
    , m_aiContribution(0.0f)
{
    loadCheckpoint();
}

JobManager::~JobManager() {
    stop();
    saveCheckpoint();
}

void JobManager::start() {
    m_running = true;
    std::thread([this]() { fetchIANoncesBackground(); }).detach();
    std::thread([this]() { saveCheckpoint(); }).detach();
}

void JobManager::stop() {
    m_running = false;
    m_cv.notify_all();
}

std::vector<uint8_t> JobManager::getCurrentBlob() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentJob.blob;
}

uint64_t JobManager::getCurrentTarget() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentJob.target;
}

std::string JobManager::getCurrentJobId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentJob.jobId;
}

uint32_t JobManager::getCurrentHeight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentJob.height;
}

void JobManager::setJob(const std::vector<uint8_t>& blob, const std::string& jobId, uint64_t target, uint32_t height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentJob.blob = blob;
    m_currentJob.jobId = jobId;
    m_currentJob.target = target;
    m_currentJob.height = height;
    
    saveCheckpoint();
}

void JobManager::setAIContribution(float contribution) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aiContribution = contribution;
}

float JobManager::getAIContribution() const {
    return m_aiContribution;
}

void JobManager::setIAEndpoint(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_iaEndpoint = endpoint;
}

std::string JobManager::getIAEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_iaEndpoint;
}

void JobManager::injectIANonces(const std::vector<uint32_t>& nonces) {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_iaQueue.size() >= MAX_QUEUE_SIZE) {
        m_iaQueue = std::queue<Nonce>();
    }
    
    for (uint32_t nonce : nonces) {
        m_iaQueue.push({nonce, m_currentJob.jobId});
    }
    m_iaContributed += nonces.size();
    
    m_cv.notify_one();
}

void JobManager::processNonces() {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Process CPU nonces
    while (!m_cpuQueue.empty()) {
        auto nonce = m_cpuQueue.front();
        m_cpuQueue.pop();
        m_processedCount++;
        
        // Validate nonce here
        bool isValid = true; // Replace with actual validation
        
        if (isValid) {
            submitValidNonce(nonce.value, nonce.jobId);
        }
    }
    
    // Process IA nonces
    while (!m_iaQueue.empty()) {
        auto nonce = m_iaQueue.front();
        m_iaQueue.pop();
        m_processedCount++;
        
        // Validate nonce here
        bool isValid = true; // Replace with actual validation
        
        if (isValid) {
            submitValidNonce(nonce.value, nonce.jobId);
        }
    }
    
    m_validNonces++;
    m_validNoncesSinceLog++;

    // Log rotation
    if (m_validNoncesSinceLog >= LOG_ROTATE_EVERY) {
        Logger::info("Processed " + std::to_string(m_validNoncesSinceLog) + " valid nonces");
        m_validNoncesSinceLog = 0;
    }

    // Export status
    m_statusExporter.exportStatusJSON(
        m_cpuQueue.size(),
        m_iaQueue.size(),
        m_validNonces,
        m_processedCount
    );
}

void JobManager::loadCheckpoint() {
    try {
        std::ifstream file("checkpoint.dat", std::ios::binary);
        if (!file) return;

        // Read checkpoint data
        std::vector<uint8_t> blob;
        std::string jobId;
        uint64_t target;
        uint32_t height;

        size_t blobSize;
        file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));
        blob.resize(blobSize);
        file.read(reinterpret_cast<char*>(blob.data()), blobSize);

        size_t jobIdSize;
        file.read(reinterpret_cast<char*>(&jobIdSize), sizeof(jobIdSize));
        jobId.resize(jobIdSize);
        file.read(&jobId[0], jobIdSize);

        file.read(reinterpret_cast<char*>(&target), sizeof(target));
        file.read(reinterpret_cast<char*>(&height), sizeof(height));

        setJob(blob, jobId, target, height);
    }
    catch (const std::exception& e) {
        Logger::error("Error loading checkpoint: " + std::string(e.what()));
    }
}

void JobManager::saveCheckpoint() {
    try {
        std::ofstream file("checkpoint.dat", std::ios::binary);
        if (!file) return;

        std::lock_guard<std::mutex> lock(m_mutex);

        // Write current job data
        size_t blobSize = m_currentJob.blob.size();
        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));
        file.write(reinterpret_cast<const char*>(m_currentJob.blob.data()), blobSize);

        size_t jobIdSize = m_currentJob.jobId.size();
        file.write(reinterpret_cast<const char*>(&jobIdSize), sizeof(jobIdSize));
        file.write(m_currentJob.jobId.c_str(), jobIdSize);

        file.write(reinterpret_cast<const char*>(&m_currentJob.target), sizeof(m_currentJob.target));
        file.write(reinterpret_cast<const char*>(&m_currentJob.height), sizeof(m_currentJob.height));
    }
    catch (const std::exception& e) {
        Logger::error("Error saving checkpoint: " + std::string(e.what()));
    }
}

void JobManager::submitValidNonce(uint32_t nonce, const std::string& jobId) {
    // Implement nonce submission logic here
    Logger::info("Valid nonce found: " + std::to_string(nonce) + " for job: " + jobId);
}

void JobManager::fetchIANoncesBackground() {
    while (m_running) {
        if (!m_iaEndpoint.empty()) {
            try {
                // Implement IA nonce fetching logic here
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (const std::exception& e) {
                Logger::error("Error fetching IA nonces: " + std::string(e.what()));
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void JobManager::submitNonce(uint32_t nonce) {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cpuQueue.push({nonce, m_currentJob.jobId});
    m_cv.notify_one();
}