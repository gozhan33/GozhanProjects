#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>

/**
 * @brief Computes SHA-256 hash of input string and returns it as a hexadecimal string
 * @param input The string to hash
 * @return Hexadecimal string representation of the SHA-256 hash
 */
std::string sha256str(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);

    return ss.str();
}

/**
 * @brief Computes SHA-256 hash of input data and returns raw bytes
 * @param data The string to hash
 * @return Vector containing the raw SHA-256 hash bytes
 */
std::vector<unsigned char> sha256(const std::string& data) {
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash.data());
    return hash;
}

/**
 * @brief Generates a new RSA key pair for cryptographic operations
 * @details Creates a 2048-bit RSA key pair using the RSA_F4 (65537) exponent
 * @return Pointer to newly allocated RSA structure, or nullptr on failure
 * @note Caller is responsible for freeing the returned RSA pointer
 */
RSA* generate_keypair() {
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4); // RSA_F4 = 65537 (standard RSA exponent)
    if (RSA_generate_key_ex(rsa, 2048, bn, nullptr) != 1) {
        RSA_free(rsa);
        BN_free(bn);
        return nullptr;
    }
    BN_free(bn);
    return rsa;
}

/**
 * @brief Signs a payload using RSA-SHA256
 * @param rsa RSA key pair to use for signing
 * @param payload Data to sign
 * @return Vector containing the signature bytes
 */
std::vector<unsigned char> sign_payload(RSA* rsa, const std::string& payload) {
    std::vector<unsigned char> hash = sha256(payload);
    std::vector<unsigned char> signature(RSA_size(rsa));
    unsigned int sig_len;
    RSA_sign(NID_sha256, hash.data(), hash.size(), signature.data(), &sig_len, rsa);
    signature.resize(sig_len);
    return signature;
}

/**
 * @brief Verifies an RSA signature using SHA-256
 * @param rsa The RSA public key for verification
 * @param payload The original message that was signed
 * @param signature The signature to verify
 * @return true if signature is valid, false otherwise
 * @note Uses SHA-256 as the message digest algorithm
 */
bool verify_signature(RSA* rsa, const std::string& payload, const std::vector<unsigned char>& signature) {
    std::vector<unsigned char> hash = sha256(payload);
    return RSA_verify(NID_sha256, hash.data(), hash.size(), signature.data(), signature.size(), rsa);
}

/**
 * @brief Converts a byte array to its hexadecimal string representation
 * @param data The byte array to convert
 * @return Hexadecimal string where each byte is represented by two characters
 * @details Each byte is formatted as exactly two hexadecimal digits with leading zeros
 */
std::string to_hex(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (unsigned char byte : data)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return oss.str();
}

/**
 * @brief Extracts an RSA key in hexadecimal format
 * @param pkey The EVP_PKEY structure containing the key
 * @param is_private true to extract private key, false for public key
 * @return Hexadecimal string representation of the key in PEM format
 * @details Converts the key to PEM format first, then to hex. For private keys,
 *          outputs unencrypted PEM (no password protection)
 */
std::string extract_key_hex(EVP_PKEY* pkey, bool is_private) {
    BIO* bio = BIO_new(BIO_s_mem());
    if (is_private)
        PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    else
        PEM_write_bio_PUBKEY(bio, pkey);

    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    std::string pem(mem->data, mem->length);
    BIO_free(bio);

    std::vector<unsigned char> bytes(pem.begin(), pem.end());
    return to_hex(bytes);
}

/**
 * @brief Prints OpenSSL error queue to stderr
 * @details Iterates through all errors in OpenSSL's error queue,
 *          printing each one with a descriptive message.
 *          The error queue is emptied after printing.
 */
void print_openssl_error() {
    unsigned long err = ERR_get_error();
    while (err) {
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        fprintf(stderr, "OpenSSL error: %s\n", buf);
        err = ERR_get_error();
    }
}

/**
 * @brief Converts a hexadecimal string to bytes
 * @param hex The hexadecimal string to convert
 * @return Vector of bytes represented by the hex string
 * @details Assumes hex string has even length and no spaces.
 *          Each pair of hex characters represents one byte.
 */
std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> out;
    out.reserve(hex.size()/2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned int byte = 0;
        sscanf(hex.c_str() + i, "%2x", &byte);
        out.push_back(static_cast<unsigned char>(byte));
    }
    return out;
}

/**
 * @brief Loads an RSA public key from either hex or PEM format
 * @param input_hex_or_pem The key data in either hex-encoded or PEM format
 * @return Pointer to RSA structure containing the public key, or nullptr on error
 * @details Supports multiple formats:
 *          - Hex-encoded PEM
 *          - Direct PEM format (begins with '-')
 *          - Hex-encoded DER in SPKI format
 *          - Hex-encoded DER in PKCS#1 format
 * @note Caller is responsible for freeing the returned RSA pointer using RSA_free
 */
RSA* load_public_key_from_hex_or_pem(const std::string& input_hex_or_pem) {
    bool looks_like_pem = !input_hex_or_pem.empty() && input_hex_or_pem.front() == '-';
    std::vector<unsigned char> bytes;

    BIO* bio = nullptr;
    RSA* rsa = nullptr;

    if (!looks_like_pem) {
        bytes = hex_to_bytes(input_hex_or_pem);
        if (bytes.empty()) {
            std::fprintf(stderr, "hex decode produced empty buffer\n");
            return nullptr;
        }
        // If decoded bytes are ASCII PEM text
        if (!bytes.empty() && bytes.front() == static_cast<unsigned char>('-')) {
            std::string pem_text(reinterpret_cast<char*>(bytes.data()), bytes.size());
            bio = BIO_new_mem_buf(pem_text.data(), static_cast<int>(pem_text.size()));
            if (!bio) {
                std::fprintf(stderr, "BIO_new_mem_buf failed\n");
                print_openssl_error();
				bytes.clear();
                return nullptr;
            }
            rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
            if (!rsa) {
                std::fprintf(stderr, "PEM_read_bio_RSA_PUBKEY failed\n");
                print_openssl_error();
            }
            BIO_free(bio);
            return rsa;
        }
    } else {
        bio = BIO_new_mem_buf(input_hex_or_pem.data(), static_cast<int>(input_hex_or_pem.size()));
        if (!bio) {
            std::fprintf(stderr, "BIO_new_mem_buf failed\n");
            print_openssl_error();
            return nullptr;
        }
        rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
        if (!rsa) {
            std::fprintf(stderr, "PEM_read_bio_RSA_PUBKEY failed\n");
            print_openssl_error();
        }
        BIO_free(bio);
        return rsa;
    }

    // At this point we have raw DER bytes in `bytes`
    const unsigned char* p = bytes.data();
    rsa = d2i_RSA_PUBKEY(nullptr, &p, static_cast<long>(bytes.size())); // SPKI DER
    if (!rsa) {
        std::fprintf(stderr, "d2i_RSA_PUBKEY failed, trying d2i_RSAPublicKey\n");
        print_openssl_error();
        // try PKCS#1 RSAPublicKey form
        p = bytes.data();
        rsa = d2i_RSAPublicKey(nullptr, &p, static_cast<long>(bytes.size()));
        if (!rsa) {
            std::fprintf(stderr, "d2i_RSAPublicKey also failed\n");
            print_openssl_error();
			bytes.clear();
            return nullptr;
        }
    }
	bytes.clear();
    return rsa;
}

/**
 * @brief Represents a transaction in the blockchain
 * 
 * A Transaction represents a transfer of value between two addresses in the blockchain.
 * Each transaction contains the sender's address (fromAddress), recipient's address (toAddress),
 * and the amount being transferred. Transactions are cryptographically signed to ensure
 * authenticity and are immutable once added to the blockchain.
 */
struct Transaction
{
public:
    /**
     * @brief Constructs a new transaction
     * @param from The sender's address (public key in hex format)
     * @param to The recipient's address (public key in hex format)
     * @param amt The amount to transfer
     * @note The transaction is not valid until signed by the sender
     */
    Transaction(const std::string& from, const std::string& to, double amt)
        : fromAddress(from), toAddress(to), amount(amt) {
            hashTX = calculateHash();
        }

    // Helper swap function
    friend void swap(Transaction& first, Transaction& second) noexcept {
        // Lock both mutexes in a fixed order to prevent deadlock
        std::scoped_lock lock(first.signatureMutex, second.signatureMutex);
        
        using std::swap;
        swap(first.fromAddress, second.fromAddress);
        swap(first.toAddress, second.toAddress);
        swap(first.amount, second.amount);
        swap(first.hashTX, second.hashTX);
        swap(first.signature, second.signature);
    }

    // Copy constructor
    Transaction(const Transaction& other)
        : Transaction(other.fromAddress, other.toAddress, other.amount) // Use primary constructor
    {
        std::scoped_lock lock(signatureMutex, other.signatureMutex);
        hashTX = other.hashTX;
        signature = other.signature;
    }

    // Copy assignment using copy-and-swap idiom
    Transaction& operator=(Transaction other) noexcept {  // Pass by value for implicit copy
        swap(*this, other);  // Swap with the copy
        return *this;        // Original data destroyed with the temporary
    }

    // Move constructor
    Transaction(Transaction&& other) noexcept
        : Transaction("", "", 0.0)  // Create empty transaction
    {
        swap(*this, other);  // Swap with the moved-from object
    }

    // Move assignment handled by copy-and-swap idiom
    // No need for separate move assignment operator!
	
    /**
     * @brief Gets the sender's address
     * @return The sender's public key in hex format
     * @thread_safety Thread-safe, accesses immutable data
     */
	std::string getFromAddress() const { 
		return fromAddress; 
	}

    /**
     * @brief Gets the recipient's address
     * @return The recipient's public key in hex format
     * @thread_safety Thread-safe, accesses immutable data
     */
	std::string getToAddress() const { 
		return toAddress; 
	}

    /**
     * @brief Gets the transaction amount
     * @return The amount being transferred
     * @thread_safety Thread-safe, accesses immutable data
     */
	double getAmount() const { 
		return amount; 
	}
	
    /**
     * @brief Calculates the unique hash for this transaction
     * @return SHA-256 hash of transaction data as hex string
     * @thread_safety Thread-safe, uses only immutable data
     */
	std::string calculateHash() const
	{
		return sha256str(fromAddress + toAddress + std::to_string(amount));
	}

    /**
     * @brief Gets the transaction signature in hexadecimal format
     * @return Hex string representation of the signature
     * @thread_safety Thread-safe, uses shared lock
     */
	std::string getSignatureHex() const
	{
		std::shared_lock lock(signatureMutex);
		return to_hex(signature);
	}

    /**
     * @brief Signs the transaction with the sender's private key
     * @param privateKey RSA private key for signing
     * @thread_safety Thread-safe, uses appropriate locks
     * @details Creates a SHA-256 hash of the transaction data and signs it
     *          using the provided RSA private key
     */
	void signTransaction(RSA* privateKey)
	{
    	// 1) compute hash off-lock as these members are only updated in the constructor
    	std::vector<unsigned char> hash = sha256(fromAddress + toAddress + std::to_string(amount));

    	// 2) prepare buffer
    	int rsa_size = RSA_size(privateKey);
    	if (rsa_size <= 0) return;
    	std::vector<unsigned char> sig(static_cast<size_t>(rsa_size));
    	unsigned int sig_len = 0;

    	// 3) serialize access to the RSA private key only
		static std::mutex rsaMutex;
    	{
        	std::lock_guard<std::mutex> key_lk(rsaMutex);
        	if (RSA_sign(NID_sha256, hash.data(), static_cast<unsigned int>(hash.size()), sig.data(), &sig_len, privateKey) != 1) 
			{
            	print_openssl_error();
            	return;
        	}
    	}
    	sig.resize(sig_len);

    	// 4) store result under signature lock
    	{
        	std::unique_lock lock(signatureMutex);
        	signature = sig;
    	}
	}
	

    /**
     * @brief Verifies the transaction's validity
     * @return true if the transaction is valid, false otherwise
     * @details Validates the transaction signature using the sender's public key.
     *          Mining reward transactions (empty fromAddress) are always valid.
     * @thread_safety Thread-safe
     */
	bool isValid() const
	{
		if (fromAddress.empty()) {
			// No from address means it's a mining reward or similar transaction
			return true;
		}

		RSA* publicKey = load_public_key_from_hex_or_pem(getFromAddress());
		bool checkIfValid = isValid(publicKey);
		RSA_free(publicKey);
		return checkIfValid;
	}

private:
    /// Sender's public key in hex format
	std::string fromAddress;
    /// Recipient's public key in hex format
	std::string toAddress;
    /// Amount to transfer
	double amount;
    /// SHA-256 hash of the transaction data
	std::string hashTX;
    /// RSA signature of the transaction hash
	std::vector<unsigned char> signature;
    /// Mutex for thread-safe access to signature
	mutable std::shared_mutex signatureMutex;

    /**
     * @brief Internal method to verify transaction signature
     * @param publicKey RSA public key for signature verification
     * @return true if signature is valid, false otherwise
     * @thread_safety Thread-safe, uses shared lock for signature access
     * @note Assumes ownership of publicKey is managed by caller
     */
	bool isValid(RSA* publicKey) const
	{
		if (!publicKey) {
			return false;  // Invalid key provided
		}

		std::vector<unsigned char> localSignature;
		{
			std::shared_lock lock(signatureMutex);
			if (signature.empty()) {
				return false;
			}
			localSignature = signature;  // Make a local copy under lock
		}

		std::vector<unsigned char> hash = sha256(getFromAddress() + getToAddress() + std::to_string(getAmount()));
		
		int verify_result = RSA_verify(NID_sha256, 
									 hash.data(), hash.size(),
									 localSignature.data(), localSignature.size(), 
									 publicKey);
		
		if (verify_result < 0) {
			// Handle error condition
			print_openssl_error();  // Print OpenSSL error queue
			return false;
		}

		return verify_result == 1;  // 1 for valid, 0 for invalid
	}
};

class Block
{

public:

	Block(const std::string& prevHash, const std::vector<Transaction>& txs)
		: previousHash(prevHash), transactions(txs), timestamp(time(nullptr)), nonce(0) {
		hash = calculateHash();
	}

	friend void swap(Block& first, Block& second) noexcept {
		std::scoped_lock lock(first.hashMutex, second.hashMutex);
		using std::swap;
		swap(first.previousHash, second.previousHash);
		swap(first.transactions, second.transactions);
		swap(first.timestamp, second.timestamp);
		swap(first.nonce, second.nonce);
		swap(first.hash, second.hash);
	}

	// Thread-safe deep copy constructor
	Block(const Block& other) : previousHash(), transactions(), timestamp(0), nonce(0), hash()
	{
    	std::shared_lock src_lock(other.hashMutex);
    	previousHash = other.previousHash;
    	transactions = other.transactions;
    	timestamp = other.timestamp;
    	nonce = other.nonce;
    	hash = other.hash;
	}

	// Thread-safe deep copy assignment using copy-and-swap idiom
    Block& operator=(Block other) noexcept {  // Pass by value for implicit copy
        swap(*this, other);  // Swap with the copy
        return *this;        // Original data destroyed with the temporary
    }

	// Move constructor
    Block(Block&& other) noexcept
       : previousHash(), transactions(), timestamp(0), nonce(0), hash()

    {
        // swap will lock both mutexes and leave `other` valid (but possibly empty)
        swap(*this, other);
    }

    /**
     * @brief Calculates the hash of the block using current nonce
     * @return SHA-256 hash of the block's contents
     * @thread_safety Thread-safe, uses shared lock
     */
	std::string calculateHash() const {
		std::shared_lock lock(hashMutex);
		std::string txData;
		for (const auto& tx : transactions) {
			txData += tx.calculateHash();
		}
		return sha256str(previousHash + txData + std::to_string(timestamp) + std::to_string(nonce));
	}

    /**
     * @brief Calculates the hash of the block using a specified nonce
     * @param nonce The nonce value to use in hash calculation
     * @return SHA-256 hash of the block's contents
     * @thread_safety Thread-safe, uses shared lock
     */
	std::string calculateHash(long nonce) const {
		std::shared_lock lock(hashMutex);
		std::string txData;
		for (const auto& tx : transactions) {
			txData += tx.calculateHash();
		}
		return sha256str(previousHash + txData + std::to_string(timestamp) + std::to_string(nonce));
	}

    /**
     * @brief Verifies that all transactions in the block are valid
     * @return true if all transactions are valid, false otherwise
     * @thread_safety Thread-safe, uses shared lock
     */
	bool hasValidTransactions() const {
		std::shared_lock lock(hashMutex);
		for (const auto& tx : transactions) {
			if (!tx.isValid()) {
				return false;
			}
		}
		return true;
	}

    /**
     * @brief Gets the current hash of the block
     * @return The block's hash as a string
     * @thread_safety Thread-safe, uses shared lock
     */
	std::string getHash() const {
		std::shared_lock lock(hashMutex);
		return hash;
	}

    /**
     * @brief Gets the hash of the previous block
     * @return The previous block's hash as a string
     * @thread_safety Thread-safe, uses shared lock
     */
	std::string getPreviousHash() const {
		std::shared_lock lock(hashMutex);
		return previousHash;
	}
	
    /**
     * @brief Gets a copy of all transactions in the block
     * @return Vector of transactions
     * @thread_safety Thread-safe, uses shared lock
     */
	std::vector<Transaction> getTransactions() const {
		std::shared_lock lock(hashMutex);
		return transactions;
	}

    /**
     * @brief Mines the block by finding a hash with the required number of leading zeros
     * @param difficulty The number of leading zeros required in the hash
     * @details Uses proof-of-work algorithm to find a nonce that produces a hash
     *          with the specified number of leading zeros
     * @thread_safety Thread-safe, uses atomic operations and proper locking
     */
	void mineBlock(int difficulty) {
    	std::string str(difficulty, '0');
    	std::atomic<long> local_nonce{0};  // Thread-safe nonce
    
    	while (hash.substr(0, difficulty) != str) {
        	local_nonce++;
        	hash = calculateHash(local_nonce);
    	}
		{
			std::unique_lock lock(hashMutex);
    		nonce = local_nonce.load();
		}
		std::cout << "Block mined: " << hash << std::endl;
	}

private:
    // Block properties
	std::string previousHash;               // Hash of the previous block in the chain
	std::vector<Transaction> transactions;  // List of transactions in this block
	long timestamp;                         // Block creation timestamp
	long nonce;                             // Proof-of-work nonce value
	mutable std::shared_mutex hashMutex;    // Mutex for thread-safe access
	std::string hash;                       // Current block's hash
};

/**
 * @brief Represents the main blockchain data structure
 * 
 * The BlockChain class manages a chain of blocks, each containing multiple transactions.
 * It handles mining new blocks, adding transactions, and maintaining the integrity of the chain.
 * The blockchain uses proof-of-work consensus and supports mining rewards.
 */
class BlockChain
{
public:
    /**
     * @brief Constructs a new blockchain with genesis block
     * @details Initializes the chain with a genesis block, sets mining difficulty
     *          and mining reward values. Uses thread-safe initialization.
     */
	BlockChain() {
		difficulty = 4;
		miningReward = 100;
		std::unique_lock lock(chainMutex);
		chain.emplace_back(createGenesisBlock());
	}

    /**
     * @brief Adds a single transaction to the pending transactions pool
     * @param transaction The transaction to add
     * @throws std::runtime_error if the transaction is invalid
     * @thread_safety Thread-safe, uses unique lock
     * @note Mining reward transactions (empty fromAddress) bypass validation
     */
	void addTransaction(const Transaction& transaction) {
		if (!transaction.getFromAddress().empty() && !transaction.getToAddress().empty()) {
			if (!transaction.isValid()) {
				throw std::runtime_error("Cannot add invalid transaction to chain");
			}
		}
		std::unique_lock lock(chainMutex);
		pendingTransactions.push_back(transaction);
	}

    /**
     * @brief Adds multiple transactions to the pending transactions pool
     * @param transactions Vector of transactions to add
     * @thread_safety Thread-safe, delegates to addTransaction
     */
	void addTransactions(const std::vector<Transaction>& transactions) {
		for (const auto& tx : transactions) {
			addTransaction(tx);
		}
	}

    /**
     * @brief Mines a new block with pending transactions and adds mining reward
     * @param miningRewardAddress Address to receive the mining reward
     * @details Creates a reward transaction, adds it to pending transactions,
     *          creates a new block, mines it with proof-of-work, and adds it to the chain
     * @thread_safety Thread-safe, uses appropriate locks for shared data
     */
	void minePendingTransactions(const std::string& miningRewardAddress) {
        Transaction rewardTx("", miningRewardAddress, miningReward);
		{
			std::unique_lock lock(chainMutex);
    		pendingTransactions.push_back(rewardTx);
            std::cout << "Added $" << miningReward << " mining rewards to: " << miningRewardAddress << std::endl;
		}
        
        Block newBlock(getLatestBlock().getHash(), pendingTransactions);
        newBlock.mineBlock(difficulty);

		{
			std::unique_lock lock(chainMutex);
        	chain.push_back(newBlock);
        	pendingTransactions.clear();
		}
	}

    /**
     * @brief Calculates the balance of a specific address
     * @param address The address to check
     * @return The current balance considering all transactions in the chain
     * @thread_safety Thread-safe, uses shared lock
     * @details Iterates through all blocks and transactions to sum up
     *          incoming and outgoing transactions for the address
     */
	double getBalanceOfAddress(const std::string& address) const {
		std::shared_lock lock(chainMutex);
		double balance = 0.0;
		for (const auto& block : chain) {
			for (const auto& tx : block.getTransactions()) {
				if (tx.getFromAddress() == address) {
					balance -= tx.getAmount();
				}
				if (tx.getToAddress() == address) {
					balance += tx.getAmount();
				}
			}
		}
		return balance;
	}

    /**
     * @brief Gets the most recently added block in the chain
     * @return Copy of the latest block
     * @thread_safety Thread-safe, uses shared lock
     */
	Block getLatestBlock() const {
		std::shared_lock lock(chainMutex);
		return chain.back();
	}

    /**
     * @brief Validates the entire blockchain
     * @return true if the chain is valid, false otherwise
     * @thread_safety Thread-safe, uses shared lock
     * @details Checks block hashes, previous block links,
     *          and transaction validity in each block
     */
	bool isChainValid() const {
		std::shared_lock lock(chainMutex);
		for (size_t i = 1; i < chain.size(); ++i) {
			const Block& currentBlock = chain[i];
			const Block& previousBlock = chain[i - 1];

			if (!currentBlock.hasValidTransactions()) {
				return false;
			}

			if (currentBlock.calculateHash() != currentBlock.getHash()) {
				return false;
			}

			if (currentBlock.getPreviousHash() != previousBlock.getHash()) {
				return false;
			}
		}
		return true;
	}

private:
    /**
     * @brief Creates the genesis (first) block of the blockchain
     * @return A new Block instance with no transactions
     */
	Block createGenesisBlock() {
		return Block("01/01/2025", {});
	}

    std::vector<Block> chain;                      // The main blockchain
    std::vector<Transaction> pendingTransactions;  // Transactions waiting to be mined
    int difficulty;                                // Current mining difficulty
    long miningReward;                             // Reward for mining a block
    mutable std::shared_mutex chainMutex;          // Mutex for thread-safe chain access
};


int main()
{
	std::cout << "Welcome to Block Chain Demo" << std::endl;

	// Initialize OpenSSL
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);

	RSA* rsaKeyForAlice = generate_keypair();
    EVP_PKEY* pkeyAlice = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkeyAlice, rsaKeyForAlice); // Transfers ownership to EVP_PKEY
	std::cout << "\nPublic Key (Hex) for Alice:\n" << extract_key_hex(pkeyAlice, false) << "\n";
    std::cout << "\nPrivate Key (Hex) for Alice:\n" << extract_key_hex(pkeyAlice, true) << "\n";

	RSA* rsaKeyForBob = generate_keypair();
	EVP_PKEY* pkeyBob = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkeyBob, rsaKeyForBob); // Transfers ownership to EVP_PKEY
	std::cout << "\nPublic Key (Hex) for Bob:\n" << extract_key_hex(pkeyBob, false) << "\n";

	// Send money from Alice to Bob
	Transaction tx1(extract_key_hex(pkeyAlice, false), extract_key_hex(pkeyBob, false), 100.0);
	tx1.signTransaction(rsaKeyForAlice);
	std::cout << "Transaction Signature (Hex): " << tx1.getSignatureHex() << std::endl;
	tx1.isValid() ? 
		std::cout << "Transaction is valid!" << std::endl : 
		std::cout << "Transaction is invalid!" << std::endl;

	// Send more money from Alice to Bob and add to blockchain
	Transaction tx2(extract_key_hex(pkeyAlice, false), extract_key_hex(pkeyBob, false), 50.0);
	tx2.signTransaction(rsaKeyForAlice);

	std::vector<Transaction> tx = { tx1, tx2 };

	BlockChain myBlock;
	std::cout << "Blockchain initialized." << std::endl;
	myBlock.addTransactions(tx);
	myBlock.minePendingTransactions(extract_key_hex(pkeyAlice, false));
	std::cout << "Block added to blockchain." << std::endl;

	// Send some more money from Alice to Bob and add to blockchain
	Transaction tx3(extract_key_hex(pkeyAlice, false), extract_key_hex(pkeyBob, false), 25.0);
	tx3.signTransaction(rsaKeyForAlice);
	myBlock.addTransaction(tx3);
	myBlock.minePendingTransactions(extract_key_hex(pkeyAlice, false));
	std::cout << "Another block added to blockchain." << std::endl;
	// Validate the blockchain
	if (myBlock.isChainValid()) {	
		std::cout << "Blockchain is valid." << std::endl;
	} else {
		std::cout << "Blockchain is invalid!" << std::endl;
	}	

	std::cout << "Alice's balance: " << myBlock.getBalanceOfAddress(extract_key_hex(pkeyAlice, false)) << std::endl;
	std::cout << "Bob's balance: " << myBlock.getBalanceOfAddress(extract_key_hex(pkeyBob, false)) << std::endl;

    EVP_PKEY_free(pkeyAlice); // Frees RSA for Alice as well
	EVP_PKEY_free(pkeyBob);   // Frees RSA for Bob as well

	// Cleanup OpenSSL
	OPENSSL_cleanup();

	return 0;
}