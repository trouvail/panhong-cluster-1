#include <iostream>
#include <fstream>
#include <sstream>
#include <paillier.hh>

using namespace std;

void encryptFile(const string &sourcePath, const string &destinationPath, const PaillierPublicKey &publicKey)
{
    ifstream sourceFile(sourcePath, ios::binary);
    ofstream destinationFile(destinationPath, ios::binary);

    if (!sourceFile.is_open() || !destinationFile.is_open())
    {
        cerr << "Error opening files." << endl;
        return;
    }

    // Read the plaintext from the source file
    stringstream plaintextStream;
    plaintextStream << sourceFile.rdbuf();
    string plaintext = plaintextStream.str();

    // Encrypt the plaintext using Paillier encryption
    vector<mpz_class> encryptedData;
    Paillier::encrypt(encryptedData, publicKey, plaintext);

    // Write the encrypted data to the destination file
    for (const mpz_class &encryptedValue : encryptedData)
    {
        destinationFile << encryptedValue << " ";
    }

    cout << "Encryption completed." << endl;
}

void decryptFile(const string &sourcePath, const string &destinationPath, const PaillierPrivateKey &privateKey)
{
    ifstream sourceFile(sourcePath);
    ofstream destinationFile(destinationPath);

    if (!sourceFile.is_open() || !destinationFile.is_open())
    {
        cerr << "Error opening files." << endl;
        return;
    }

    // Read the encrypted data from the source file
    vector<mpz_class> encryptedData;
    mpz_class encryptedValue;
    while (sourceFile >> encryptedValue)
    {
        encryptedData.push_back(encryptedValue);
    }

    // Decrypt the encrypted data using Paillier decryption
    string decryptedData;
    Paillier::decrypt(decryptedData, privateKey, encryptedData);

    // Write the decrypted data to the destination file
    destinationFile << decryptedData;

    cout << "Decryption completed." << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " <sourceFilePath> <destinationFilePath> <encrypt/decrypt>" << endl;
        return 1;
    }

    string sourcePath = argv[1];
    string destinationPath = argv[2];
    string operation = argv[3];

    if (operation != "encrypt" && operation != "decrypt")
    {
        cerr << "Invalid operation. Use 'encrypt' or 'decrypt'." << endl;
        return 1;
    }

    // Generate Paillier key pair
    PaillierKeyPair keyPair;
    keyPair.GenerateKeys();

    if (operation == "encrypt")
    {
        encryptFile(sourcePath, destinationPath, keyPair.publicKey);
    }
    else
    {
        decryptFile(sourcePath, destinationPath, keyPair.privateKey);
    }

    return 0;
}

// g++ test-tran-c.cpp -o test-tran-c
// ./test-tran-c <sourceFilePath> <destinationFilePath> <encrypt/decrypt>