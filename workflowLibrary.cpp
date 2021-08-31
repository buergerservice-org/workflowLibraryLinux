// workflowLibrary.cpp
// read data of german Personalausweis with selfauthentication from AusweisApp2
// Copyright (C) 2021 buergerservice.org e.V. <KeePerso@buergerservice.org>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)


#define BOOST_ALL_NO_LIB

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h> //the openssl-license https://www.openssl.org/source/license.html
#include <openssl/sha.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "json/json.h" //jsoncpp uses MIT-Licence

#include "workflowLibrary.h"
#include <cstdlib>
#include <iostream>
//#include <stdio.h>
//#include <stdlib.h>
#include <iomanip>
//#include <time.h>
//#include <synchapi.h>
//#include <chrono>
//#include <thread>
#include <stdlib.h>



namespace workflowLibrary
{

    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
    using response_type = http::response<http::string_body>;

    typedef ssl::stream<tcp::socket> ssl_socket;



    void findAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr)
    {
        // Get the first occurrence
        size_t pos = data.find(toSearch);
        // Repeat till end is reached
        while (pos != std::string::npos)
        {
            // Replace this occurrence of Sub String
            data.replace(pos, toSearch.size(), replaceStr);
            // Get the next occurrence from the current position
            pos = data.find(toSearch, pos + replaceStr.size());
        }
    }

    void findAndReplaceBehind(std::string& data, std::string toSearch, std::string replaceStr)
    {
        // Get the first occurrence
        size_t pos = data.find(toSearch);

        // Replace this occurrence of Sub String
        data.replace(pos + toSearch.size(), 2, replaceStr);
    }


    void handleErrors(void)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }

    // from https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
    int  workflow::encrypt(unsigned char* plaintext, int plaintext_len, unsigned char* key,
        unsigned char* iv, unsigned char* ciphertext)
    {
        EVP_CIPHER_CTX* ctx;

        int len;

        int ciphertext_len;

        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new()))
            handleErrors();

        /*
         * Initialise the encryption operation. IMPORTANT - ensure you use a key
         * and IV size appropriate for your cipher
         * In this example we are using 256 bit AES (i.e. a 256 bit key). The
         * IV size for *most* modes is the same as the block size. For AES this
         * is 128 bits
         */
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
            handleErrors();

        /*
         * Provide the message to be encrypted, and obtain the encrypted output.
         * EVP_EncryptUpdate can be called multiple times if necessary
         */
        if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
            handleErrors();
        ciphertext_len = len;

        /*
         * Finalise the encryption. Further ciphertext bytes may be written at
         * this stage.
         */
        if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
            handleErrors();
        ciphertext_len += len;

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        return ciphertext_len;
    }

    // from https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
    int  workflow::decrypt(unsigned char* ciphertext, int ciphertext_len, unsigned char* key,
        unsigned char* iv, unsigned char* plaintext)
    {
        EVP_CIPHER_CTX* ctx;

        int len;

        int plaintext_len;

        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new()))
            handleErrors();

        /*
         * Initialise the decryption operation. IMPORTANT - ensure you use a key
         * and IV size appropriate for your cipher
         * In this example we are using 256 bit AES (i.e. a 256 bit key). The
         * IV size for *most* modes is the same as the block size. For AES this
         * is 128 bits
         */
        if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
            handleErrors();

        /*
         * Provide the message to be decrypted, and obtain the plaintext output.
         * EVP_DecryptUpdate can be called multiple times if necessary.
         */
        if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
            handleErrors();
        plaintext_len = len;

        /*
         * Finalise the decryption. Further plaintext bytes may be written at
         * this stage.
         */
        if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
            handleErrors();
        plaintext_len += len;

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        return plaintext_len;
    }


    void  workflow::BIO_dump_fp_wrap(FILE* fp, char const* s, int len)
    {
        BIO_dump_fp(fp, s, len);
    }


    Json::Value readjsondata(std::string inputstring)
    {
        Json::Value root;
        Json::CharReaderBuilder jsonbuilder;
        Json::CharReader* jsonreader = jsonbuilder.newCharReader();
        std::string errors;

        std::cout << "parsing jsondata." << std::endl;
        bool parsingSuccessful = jsonreader->parse(inputstring.c_str(),
            inputstring.c_str() + inputstring.length(), &root, &errors);
        //delete reader;
        if (!parsingSuccessful)
        {
            std::cerr << "jsoncpp parsing errors: " << errors << std::endl;
        }
        else
        {
            std::cerr << "jsoncpp parsing successful: " << errors << std::endl;
        }

        return root;
    }


    std::string workflow::readjson(std::string inputstring)
    {
        bool parsingSuccessful = false;

        Json::Value root;
        Json::CharReaderBuilder jsonbuilder;
        Json::CharReader* jsonreader = jsonbuilder.newCharReader();
        std::string errors;
        std::string outputstring;
        std::string bn;
        std::string bns;

        std::cout << "parsing jsondata." << std::endl;
        parsingSuccessful = jsonreader->parse(inputstring.c_str(),
            inputstring.c_str() + inputstring.length(), &root, &errors);
        //delete reader;
        if (!parsingSuccessful)
        {
            std::cerr << "jsoncpp parsing errors: " << errors << std::endl;
        }

        //std::cout << "StyledString: " << root["PersonalData"].toStyledString() << std::endl;
        personalStyledString = root["PersonalData"].toStyledString();
        //std::cout << "--------------------------" << std::endl;
        //std::cout << "Exampledata read from json" << std::endl;
        Json::Value vinfo = root["PersonalData"];

        //for hashkey
        if (vinfo["BirthName"].asString() == "") {
            outputstring = vinfo["FamilyNames"].asString() + vinfo["GivenNames"].asString() + vinfo["DateOfBirth"].asString();
        }
        else
        {
            bn = vinfo["BirthName"].asString();
            std::cout << "substr(0,4)= " << bn.substr(0, 4) << std::endl;
            if (bn.substr(0, 4) == "GEB.")
            {
                if (bn.substr(0, 4) == "GEB. ")
                {
                    bns = bn.substr(5);
                }
                else
                {
                    bns = bn.substr(4);
                }
                outputstring = bns + vinfo["GivenNames"].asString() + vinfo["DateOfBirth"].asString();
            }
            else
            {
                outputstring = vinfo["BirthName"].asString() + vinfo["GivenNames"].asString() + vinfo["DateOfBirth"].asString();
            }
        }

        AcademicTitle = vinfo["AcademicTitle"].asString();
        ArtisticName = vinfo["ArtisticName"].asString();
        BirthName = vinfo["BirthName"].asString();
        DateOfBirth = vinfo["DateOfBirth"].asString();
        DocumentType = vinfo["DocumentType"].asString();
        FamilyNames = vinfo["FamilyNames"].asString();
        GivenNames = vinfo["GivenNames"].asString();
        IssuingState = vinfo["IssuingState"].asString();
        Nationality = vinfo["Nationality"].asString();

        //std::cout << "the Familyname : " << vinfo["FamilyNames"].asString() << std::endl;
        //std::cout << "the GivenName : " << vinfo["GivenNames"].asString() << std::endl;
        //std::cout << "the DateOfBirth : " << vinfo["DateOfBirth"].asString() << std::endl;
        vinfo = root["PersonalData"]["PlaceOfBirth"];

        //for hashkey
        outputstring = outputstring + vinfo["FreetextPlace"].asString();

        PlaceOfBirth = vinfo["FreetextPlace"].asString();

        vinfo = root["PersonalData"]["PlaceOfResidence"]["StructuredPlace"];
        //outputstring = outputstring + vinfo["City"].asString() + vinfo["Street"].asString();
        City = vinfo["City"].asString();
        Country = vinfo["Country"].asString();
        Street = vinfo["Street"].asString();
        ZipCode = vinfo["ZipCode"].asString();

        //std::cout << "StyledString: " << root["PersonalData"].toStyledString() << std::endl;
        //workflow::setdatabase(root);
        return outputstring; //returns a string of four data
    }


    // https://stackoverflow.com/questions/2262386/generate-sha256-with-openssl-and-c/10632725
    std::string workflow::sha256(const std::string str)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, str.c_str(), str.size());
        SHA256_Final(hash, &sha256);
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }


    std::string workflow::getcertificate()
    {
        std::string outputstring = "";
        net::io_context ioc;
        tcp::resolver resolver{ ioc };
        websocket::stream<tcp::socket> ws{ ioc };
        //freopen("log.txt", "a+", stdout);

        try
        {

            std::string host = "127.0.0.1";
            std::string port = "24727";
            std::string target = "/eID-Kernel";

            // The io_context is required for all I/O
            //net::io_context ioc;

            // These objects perform our I/O
            //tcp::resolver resolver{ ioc };
            //websocket::stream<tcp::socket> ws{ ioc };

            // Look up the domain name
            auto const results = resolver.resolve(host, port);
            std::cout << "connect to AusweisApp2." << std::endl;
            // Make the connection on the IP address we get from a lookup
            //std::cerr << "before connect." << std::endl;
            auto ep = net::connect(ws.next_layer(), results);
            //std::cerr << "behind connect." << std::endl;
            // Update the host_ string. This will provide the value of the
            // Host HTTP header during the WebSocket handshake.
            // See https://tools.ietf.org/html/rfc7230#section-5.4
            host += ':' + std::to_string(ep.port());

            // Set a decorator to change the User-Agent of the handshake
            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(http::field::user_agent,
                        //std::string(BOOST_BEAST_VERSION_STRING) +
                        //" websocket-client-coro");
                        "buergerservice.org e.V. workflowLibraryLinux");
                }));
            response_type res;
            std::cout << "starting synchronous websocket handshake." << std::endl;
            // Perform the websocket handshake
            ws.handshake(res, host, target);

            std::stringstream strbuffer;
            strbuffer << "handshake - response: \n" << res << std::endl;
            outputstring = strbuffer.str();
            std::cout << "outputstring: " << outputstring << std::endl;
            if (outputstring.find("Error") != std::string::npos)
            {
                std::cout << "found Error, please check your cardreader!" << std::endl;
                return "e3";
            }


            // This buffer will hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            ws.read(buffer);
            // The make_printable() function helps print a ConstBufferSequence
            std::cout << beast::make_printable(buffer.data()) << std::endl;
            //std::cout << "behind first print buffer." << std::endl;
            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << "outputstring: " << outputstring << std::endl;

            //buffer.clear();
            //ws.read(buffer);
            //outputstring = beast::buffers_to_string(buffer.data());
            //std::cout << outputstring << std::endl;

            std::cout << "Send command: {\"cmd\": \"RUN_AUTH\", \"tcTokenURL\" : \"https://www.autentapp.de/AusweisAuskunft/WebServiceRequesterServlet?mode=json\"}" << std::endl;
            ws.write(net::buffer(std::string("{\"cmd\": \"RUN_AUTH\", \"tcTokenURL\" : \"https://www.autentapp.de/AusweisAuskunft/WebServiceRequesterServlet?mode=json\"}")));
            buffer.clear();
            ws.read(buffer);
            std::cout << beast::make_printable(buffer.data()) << std::endl;

            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"AUTH\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    return "e1";
                }
            }
            std::cout << "found message AUTH, ok we continue." << std::endl;

            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"ACCESS_RIGHTS\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    return "e1";
                }
            }
            std::cout << "found message ACCESS_RIGHTS, ok we continue." << std::endl;


            std::cout << "Send command: {\"cmd\": \"GET_CERTIFICATE\"}" << std::endl;
            ws.write(net::buffer(std::string("{\"cmd\": \"GET_CERTIFICATE\"}")));
            buffer.clear();
            ws.read(buffer);
            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;

            while (outputstring.find("\"CERTIFICATE\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    return "e1";
                }
            }
            std::cout << "found message CERTIFICATE, ok we continue" << std::endl;

            //findAndReplaceAll(outputstring, "\\u00fc", "ue");
            //findAndReplaceAll(outputstring, "\\u00e4", "ae");
            //findAndReplaceAll(outputstring, "\\u00df", "ss");

            Json::Value certificatedata;
            certificatedata = readjsondata(outputstring);
            std::cout << "StyledString: " << certificatedata["description"].toStyledString() << std::endl;

            std::cout << "issuerName: " << certificatedata["description"]["issuerName"].asString() << std::endl;
            std::cout << "issuerUrl: " << certificatedata["description"]["issuerUrl"].asString() << std::endl;
            std::cout << "purpose: " << certificatedata["description"]["purpose"].asString() << std::endl;
            std::cout << "subjectName: " << certificatedata["description"]["subjectName"].asString() << std::endl;
            std::cout << "subjectUrl: " << certificatedata["description"]["subjectUrl"].asString() << std::endl;
            std::cout << "termsOfUsage: " << certificatedata["description"]["termsOfUsage"].asString() << std::endl;
            std::cout << "effectiveDate: " << certificatedata["validity"]["effectiveDate"].asString() << std::endl;
            std::cout << "expirationDate: " << certificatedata["validity"]["expirationDate"].asString() << std::endl;

            certificateStyledString = certificatedata["description"].toStyledString();
            issuerName = certificatedata["description"]["issuerName"].asString();
            issuerUrl = certificatedata["description"]["issuerUrl"].asString();
            purpose = certificatedata["description"]["purpose"].asString();
            subjectName = certificatedata["description"]["subjectName"].asString();
            subjectUrl = certificatedata["description"]["subjectUrl"].asString();
            termsOfUsage = certificatedata["description"]["termsOfUsage"].asString();
            //findAndReplaceBehind(termsOfUsage, "Hinweis auf die f", "ue");
            //findAndReplaceBehind(termsOfUsage, "Diensteanbieter zust", "ae");
            //findAndReplaceBehind(termsOfUsage, "Die Landesbeauftragte f", "ue");
            //findAndReplaceBehind(termsOfUsage, "Arndtstra", "ss");
            //termsOfUsage = certificateStyledString.substr(certificateStyledString.find("termsOfUsage") + 16);
            //termsOfUsage = termsOfUsage.substr(0, termsOfUsage.find("}")-1);
            //findAndReplaceAll(termsOfUsage, "\\u00fc", "ue");
            //findAndReplaceAll(termsOfUsage, "\\u00e4", "ae");
            //findAndReplaceAll(termsOfUsage, "\\u00df", "ss");
            //findAndReplaceAll(termsOfUsage, "\\r\\n", "   ");

            effectiveDate = certificatedata["validity"]["effectiveDate"].asString();
            expirationDate = certificatedata["validity"]["expirationDate"].asString();

            // Close the WebSocket connection
            if (ws.is_open()) {
                ws.close(websocket::close_code::normal);
                std::cout << "closed websocketconnection." << std::endl;
            }

        }
        catch (std::exception const& e)
        {
            // Close the WebSocket connection
            if (ws.is_open()) {
                ws.close(websocket::close_code::normal);
                std::cout << "closed websocketconnection." << std::endl;
            }

            std::cerr << "Error: " << e.what() << std::endl;
            //return EXIT_FAILURE;
            return "e1";
        }
        //return EXIT_SUCCESS;

        //fclose(stdout);
        if (ws.is_open()) {
            ws.close(websocket::close_code::normal);
            std::cout << "closed websocketconnection." << std::endl;
        }

        return outputstring;
    }


    std::string workflow::getkeypad()
    {
        std::string outputstring = "";
        net::io_context ioc;
        tcp::resolver resolver{ ioc };
        websocket::stream<tcp::socket> ws{ ioc };
        //freopen("log.txt", "a+", stdout);

        try
        {

            std::string host = "127.0.0.1";
            std::string port = "24727";
            std::string target = "/eID-Kernel";

            // The io_context is required for all I/O
            //net::io_context ioc;

            // These objects perform our I/O
            //tcp::resolver resolver{ ioc };
            //websocket::stream<tcp::socket> ws{ ioc };

            // Look up the domain name
            auto const results = resolver.resolve(host, port);
            std::cout << "connect to AusweisApp2." << std::endl;
            // Make the connection on the IP address we get from a lookup
            //std::cerr << "before connect." << std::endl;
            auto ep = net::connect(ws.next_layer(), results);
            //std::cerr << "behind connect." << std::endl;
            // Update the host_ string. This will provide the value of the
            // Host HTTP header during the WebSocket handshake.
            // See https://tools.ietf.org/html/rfc7230#section-5.4
            host += ':' + std::to_string(ep.port());

            // Set a decorator to change the User-Agent of the handshake
            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(http::field::user_agent,
                        //std::string(BOOST_BEAST_VERSION_STRING) +
                        //" websocket-client-coro");
                        "buergerservice.org e.V. workflowLibraryLinux");
                }));

            /*
            websocket::stream_base::timeout opt{
                std::chrono::seconds(3),   // handshake timeout
                std::chrono::seconds(3),       // idle timeout. no data recv from peer for n sec, then it is timeout
                false   //no ping-pong to keep alive
            };
            // Set the timeout options on the stream.
            ws.set_option(opt);
            */

            response_type res;
            std::cout << "starting synchronous websocket handshake." << std::endl;
            // Perform the websocket handshake
            ws.handshake(res, host, target);

            std::stringstream strbuffer;
            strbuffer << "handshake - response: \n" << res << std::endl;
            outputstring = strbuffer.str();
            std::cout << "outputstring: " << outputstring << std::endl;
            if (outputstring.find("Error") != std::string::npos)
            {
                std::cout << "found Error, please check your cardreader!" << std::endl;
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                return "e3";
            }

            std::string ver1 = outputstring.substr(outputstring.find("AusweisApp2") + 12, 1);
            std::cout << "ver1: " << ver1 << std::endl;
            std::string ver2 = outputstring.substr(outputstring.find("AusweisApp2") + 14, 2);
            std::cout << "ver2: " << ver2 << std::endl;
            if (std::stoi(ver1) < 1 || std::stoi(ver2) < 22)
            {
                std::cout << "AusweisApp2-version less 1.22.*, please update to new AusweisApp2!" << std::endl;
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                return "e4";
            }
            else
            {
                std::cout << "AusweisApp2-version equal or greater 1.22.*, ok we continue." << std::endl;
            }

            // This buffer will hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            //ws.read(buffer);
            // The make_printable() function helps print a ConstBufferSequence
            //std::cout << beast::make_printable(buffer.data()) << std::endl;
            //std::cout << "behind first print buffer." << std::endl;
            //outputstring = beast::buffers_to_string(buffer.data());
            //std::cout << "outputstring: " << outputstring << std::endl;


            //Sleep(1000);
            //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            int status=system("sleep 1");
            std::cout << "Status of systemcommand sleep: " << status << std::endl;
            /*
            std::cout << "Status of systemcommand sleep: " << status << std::endl;
            std::cout << "Send command: {\"cmd\": \"GET_READER_LIST\"}" << std::endl;
            ws.write(net::buffer(std::string("{\"cmd\": \"GET_READER_LIST\"}")));

            //Sleep(1000);
            buffer.clear();
            ws.read(buffer);
            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"READER_LIST\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
            }
            std::cout << "found message READER_LIST, ok we continue" << std::endl;

            Json::Value readerdata;
            readerdata = readjsondata(outputstring);
            std::cout << "StyledString: " << readerdata["reader"].toStyledString() << std::endl;
            std::cout << "readers numbers " << readerdata["reader"].size() << std::endl;

            if (readerdata["reader"].size()==0)
            {
                std::cout << "found no cardreader, please check your cardreader!" << std::endl;
                return "e7";
            }
            else
            {
                std::cout << "cardreader found, ok we continue." << std::endl;
            }

            bool foundcardactivated = false;

            for (unsigned int i = 0; i < readerdata["reader"].size(); i++)
            {
                std::cout << "reader nr " << i << ": " << " name: " << readerdata["reader"][i]["name"] << std::endl;
                std::cout << "attached nr " << i << ": " << readerdata["reader"][i]["attached"].asString() << std::endl;
                if (readerdata["reader"][i]["attached"])
                {
                    std::cout << "   card deactivated nr " << i << ": " << readerdata["reader"][i]["card"]["deactivated"].asString() << std::endl;
                    if (readerdata["reader"][i]["card"]["deactivated"])
                    {
                        std::cout << "   card ative nr " << i << std::endl;
                        //this is the activated reader
                        std::cout << "   card retrycounter:" << readerdata["reader"][i]["card"]["retryCounter"].asString() << std::endl;
                        if (readerdata["reader"][i]["card"]["retryCounter"] == 3)
                        {
                            //this is the activated reader with retrycounter==3
                            std::cout << "keypad: " << readerdata["reader"][i]["keypad"].asString() << std::endl;
                            outputstring = readerdata["reader"][i]["keypad"].asString();
                            foundcardactivated = true;
                        }
                        else
                        {
                            std::cout << "found message retryCounter!=3, please start a selfauthentication direct with AusweisApp2!" << std::endl;
                            return "e5";
                        }
                    }
                }
            }
            */
            bool foundcardreaderactivated = false;
            bool foundcardactivated = false;

            for (int i = 0; i < 5; i++)
            {
                std::cout << "waiting for message READER turn: " << i + 1 << std::endl;
                buffer.clear();
                //ws.read(buffer);

                try {
                    ws.read(buffer);
                }
                catch (std::exception const& e)
                {
                    std::cerr << "Error: " << e.what() << std::endl;
                }

                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    std::cout << "found error, exiting." << std::endl;
                    //outputstring = "e1";
                    //return EXIT_FAILURE;
                    return "e1";
                }
                if (outputstring.find("\"msg\":\"READER\"") != std::string::npos)
                {
                    std::cout << "found message READER, ok we continue" << std::endl;
                    if (outputstring.find("\"deactivated\":false") != std::string::npos)
                    {
                        std::cout << "found deactivated:false, ok we continue" << std::endl;
                        foundcardreaderactivated = true;

                        if (outputstring.find("\"retryCounter\":") != std::string::npos)
                        {
                            std::cout << "found retryCounter:, ok we continue" << std::endl;

                            std::string retrycount = outputstring.substr(outputstring.find("retryCounter") + 14, 1);
                            if (retrycount == "3")
                            {
                                std::cout << "found message retryCounter=3, ok we continue" << std::endl;
                                std::string keypadr = outputstring.substr(outputstring.find("keypad") + 8, 5);
                                if (keypadr != "false") keypadr = "true";
                                std::cout << "found message keypad, ok we continue. keypad is:" << keypadr << std::endl;
                                outputstring = keypadr;
                                foundcardactivated = true;
                                break;
                            }
                            else
                            {
                                std::cout << "found message retryCounter!=3, please start a selfauthentication direct with AusweisApp2!" << std::endl;
                                return "e5";
                                //outputstring = "e5";
                                //return EXIT_FAILURE;
                            }
                        }
                    }
                }
                //int status = system("sleep 1");
            }


            if (!foundcardreaderactivated)
            {
                std::cout << "found no active cardreader, please check your cardreader!" << std::endl;
                //outputstring = "e7";
                // Close the WebSocket connection
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                //return EXIT_FAILURE;
                return "e7";
            }
            else
            {
                std::cout << "found active cardreader, ok we continue." << std::endl;
            }

            if (!foundcardactivated)
            {
                std::cout << "found no active card, please check your Personalausweis!" << std::endl;
                return "e2";
            }
            else
            {
                std::cout << "found message for active card, ok we continue." << std::endl;
            }

            /*
            if (outputstring.find("retryCounter\":3") == std::string::npos)
            {
                std::cout << "found message retryCounter!=3, please start a selfauthentication direct with AusweisApp2!" << std::endl;
                return "e5";
            }
            else
            {
                std::cout << "found message retryCounter:3, ok we continue." << std::endl;
            }

            if (outputstring.find("keypad\":false") != std::string::npos)
            {
                std::cout << "found keypad:false" << std::endl;
                outputstring = "false";
                //workflow::keypad = false;
            }
            if (outputstring.find("keypad\":true") != std::string::npos)
            {
                std::cout << "found keypad:true" << std::endl;
                outputstring = "true";
                //workflow::keypad = true;
            }
            */

            // Close the WebSocket connection
            if (ws.is_open()) {
                ws.close(websocket::close_code::normal);
                std::cout << "closed websocketconnection." << std::endl;
            }

        }
        catch (std::exception const& e)
        {
            // Close the WebSocket connection
            if (ws.is_open()) {
                ws.close(websocket::close_code::normal);
                std::cout << "closed websocketconnection." << std::endl;
            }


            std::cerr << "Error: " << e.what() << std::endl;
            //return EXIT_FAILURE;
            return "e1";
        }
        //return EXIT_SUCCESS;

        //fclose(stdout);
        if (ws.is_open()) {
            ws.close(websocket::close_code::normal);
            std::cout << "closed websocketconnection." << std::endl;
        }
        std::cout << "outputstring of getkeypad is: " << outputstring << std::endl;
        return outputstring;
    }




    std::string workflow::startworkflow(std::string PINstring)
    {
        std::string outputstring;
        std::string errors;

        //freopen("log.txt", "a+", stdout);


        try
        {

            std::string host = "127.0.0.1";
            std::string port = "24727";
            std::string target = "/eID-Kernel";

            // The io_context is required for all I/O
            net::io_context ioc;

            // These objects perform our I/O
            tcp::resolver resolver{ ioc };
            websocket::stream<tcp::socket> ws{ ioc };

            // Look up the domain name
            auto const results = resolver.resolve(host, port);
            std::cout << "connect to AusweisApp2." << std::endl;
            // Make the connection on the IP address we get from a lookup
            //std::cerr << "before connect." << std::endl;
            auto ep = net::connect(ws.next_layer(), results);
            //std::cerr << "behind connect." << std::endl;
            // Update the host_ string. This will provide the value of the
            // Host HTTP header during the WebSocket handshake.
            // See https://tools.ietf.org/html/rfc7230#section-5.4
            host += ':' + std::to_string(ep.port());

            // Set a decorator to change the User-Agent of the handshake
            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(http::field::user_agent,
                        //std::string(BOOST_BEAST_VERSION_STRING) +
                        //" websocket-client-coro");
                        "buergerservice.org e.V. workflowLibraryLinux");
                }));
            response_type res;
            std::cout << "starting synchronous websocket handshake." << std::endl;
            // Perform the websocket handshake
            ws.handshake(res, host, target);
            //std::cout << "handshake - response: \n" << res << std::endl;

            std::stringstream strbuffer;
            strbuffer << "handshake - response: \n" << res << std::endl;
            outputstring = strbuffer.str();
            std::cout << "outputstring: " << outputstring << std::endl;
            if (outputstring.find("Error") != std::string::npos)
            {
                std::cout << "found Error, please check your cardreader!" << std::endl;
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                return "e3";
            }

            std::string ver1 = outputstring.substr(outputstring.find("AusweisApp2") + 12, 1);
            std::cout << "ver1: " << ver1 << std::endl;
            std::string ver2 = outputstring.substr(outputstring.find("AusweisApp2") + 14, 2);
            std::cout << "ver2: " << ver2 << std::endl;
            if (std::stoi(ver1) < 1 || std::stoi(ver2) < 22)
            {
                std::cout << "AusweisApp2-version less 1.22.*, please update to new AusweisApp2!" << std::endl;
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                return "e4";
            }
            else
            {
                std::cout << "AusweisApp2-version equal or greater 1.22.*, ok we continue." << std::endl;
            }

            // This buffer will hold the incoming message
            beast::flat_buffer buffer;

            // Read a message into our buffer
            //ws.read(buffer);
            // The make_printable() function helps print a ConstBufferSequence
            //std::cout << beast::make_printable(buffer.data()) << std::endl;
            //std::cout << "behind first print buffer." << std::endl;
            //outputstring = beast::buffers_to_string(buffer.data());
            //std::cout << "outputstring: " << outputstring << std::endl;


            //Sleep(1000);
            //std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            int status=system("sleep 1");
            std::cout << "Status of systemcommand sleep: " << status << std::endl;
            /*
            std::cout << "Status of systemcommand sleep: " << status << std::endl;
            std::cout << "Send command: {\"cmd\": \"GET_READER_LIST\"}" << std::endl;
            ws.write(net::buffer(std::string("{\"cmd\": \"GET_READER_LIST\"}")));
            buffer.clear();
            ws.read(buffer);

            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"READER_LIST\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    std::cout << "found error, exiting." << std::endl;
                    //outputstring = "e1";
                    return "e1";
                }
            }
            std::cout << "found message READER_LIST, ok we continue" << std::endl;


            Json::Value readerdata;
            readerdata = readjsondata(outputstring);
            std::cout << "StyledString: " << readerdata["reader"].toStyledString() << std::endl;
            std::cout << "readers number " << readerdata["reader"].size() << std::endl;

            if (readerdata["reader"].size() == 0)
            {
                std::cout << "found no cardreader, please check your cardreader!" << std::endl;
                return "e7";
            }
            else
            {
                std::cout << "cardreader found, ok we continue." << std::endl;
            }

            bool foundcardactivated = false;

            for (unsigned int i = 0; i < readerdata["reader"].size(); i++)
            {
                std::cout << "reader nr " << i << ": " << " name: " << readerdata["reader"][i]["name"] << std::endl;
                std::cout << "attached nr " << i << ": " << readerdata["reader"][i]["attached"].asString() << std::endl;
                if (readerdata["reader"][i]["attached"])
                {
                    std::cout << "   card deactivated nr " << i << ": " << readerdata["reader"][i]["card"]["deactivated"].asString() << std::endl;
                    if (readerdata["reader"][i]["card"]["deactivated"])
                    {
                        std::cout << "   card ative nr " << i << std::endl;
                        //this is the activated reader
                        std::cout << "   card retrycounter:" << readerdata["reader"][i]["card"]["retryCounter"].asString() << std::endl;
                        if (readerdata["reader"][i]["card"]["retryCounter"] == 3)
                        {
                            //this is the activated reader with retrycounter==3
                            std::cout << "keypad: " << readerdata["reader"][i]["keypad"].asString() << std::endl;
                            outputstring = readerdata["reader"][i]["keypad"].asString();
                            foundcardactivated = true;
                        }
                        else
                        {
                            std::cout << "found message retryCounter!=3, please start a selfauthentication direct with AusweisApp2!" << std::endl;
                            //return "e5";
                            //outputstring = "e5";
                            return "e5";
                        }
                    }
                }
            }
            */

            bool foundcardreaderactivated = false;
            bool foundcardactivated = false;

            for (int i = 0; i < 5; i++)
            {
                std::cout << "waiting for message READER turn: " << i + 1 << std::endl;
                buffer.clear();
                //ws.read(buffer);

                try {
                    ws.read(buffer);
                }
                catch (std::exception const& e)
                {
                    std::cerr << "Error: " << e.what() << std::endl;
                }

                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    std::cout << "found error, exiting." << std::endl;
                    //outputstring = "e1";
                    //return EXIT_FAILURE;
                    return "e1";
                }
                if (outputstring.find("\"msg\":\"READER\"") != std::string::npos)
                {
                    std::cout << "found message READER, ok we continue" << std::endl;
                    if (outputstring.find("\"deactivated\":false") != std::string::npos)
                    {
                        std::cout << "found deactivated:false, ok we continue" << std::endl;
                        foundcardreaderactivated = true;

                        if (outputstring.find("\"retryCounter\":") != std::string::npos)
                        {
                            std::cout << "found retryCounter:, ok we continue" << std::endl;

                            std::string retrycount = outputstring.substr(outputstring.find("retryCounter") + 14, 1);
                            if (retrycount == "3")
                            {
                                std::cout << "found message retryCounter=3, ok we continue" << std::endl;
                                std::string keypadr = outputstring.substr(outputstring.find("keypad") + 8, 5);
                                if (keypadr != "false") keypadr = "true";
                                std::cout << "found message keypad, ok we continue. keypad is:" << keypadr << std::endl;
                                outputstring = keypadr;
                                foundcardactivated = true;
                                break;
                            }
                            else
                            {
                                std::cout << "found message retryCounter!=3, please start a selfauthentication direct with AusweisApp2!" << std::endl;
                                return "e5";
                                //outputstring = "e5";
                                //return EXIT_FAILURE;
                            }
                        }
                    }
                }
                //int status = system("sleep 1");
            }


            if (!foundcardreaderactivated)
            {
                std::cout << "found no active cardreader, please check your cardreader!" << std::endl;
                //outputstring = "e7";
                // Close the WebSocket connection
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                //return EXIT_FAILURE;
                return "e7";
            }
            else
            {
                std::cout << "found active cardreader, ok we continue." << std::endl;
            }

            if (!foundcardactivated)
            {
                std::cout << "found no active card, please check your Personalausweis!" << std::endl;
                //return "e2";
                //outputstring = "e2";
                if (ws.is_open()) {
                    ws.close(websocket::close_code::normal);
                    std::cout << "closed websocketconnection." << std::endl;
                }
                return "e2";
            }
            else
            {
                std::cout << "found message for active card, ok we continue." << std::endl;
            }

            // --- now the websocketconnection is online      
            std::cout << "workflow 1 - Minimal successful authentication (in docu point 6.1):\n" << std::endl;

            std::cout << "Send command: {\"cmd\": \"RUN_AUTH\", \"tcTokenURL\" : \"https://www.autentapp.de/AusweisAuskunft/WebServiceRequesterServlet?mode=json\"}" << std::endl;
            ws.write(net::buffer(std::string("{\"cmd\": \"RUN_AUTH\", \"tcTokenURL\" : \"https://www.autentapp.de/AusweisAuskunft/WebServiceRequesterServlet?mode=json\"}")));
            buffer.clear();
            ws.read(buffer);
            std::cout << beast::make_printable(buffer.data()) << std::endl;

            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"AUTH\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    if (ws.is_open()) {
                        ws.close(websocket::close_code::normal);
                        std::cout << "closed websocketconnection." << std::endl;
                    }
                    return "e1";
                }
            }
            //std::cout << "found message AUTH, ok we continue." << std::endl;


            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"ACCESS_RIGHTS\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    if (ws.is_open()) {
                        ws.close(websocket::close_code::normal);
                        std::cout << "closed websocketconnection." << std::endl;
                    }
                    return "e1";
                }
            }

            std::cout << "Send command: {\"cmd\": \"ACCEPT\"}" << std::endl;
            ws.write(net::buffer(std::string("{\"cmd\": \"ACCEPT\"}")));

            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"ENTER_PIN\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    if (ws.is_open()) {
                        ws.close(websocket::close_code::normal);
                        std::cout << "closed websocketconnection." << std::endl;
                    }
                    return "e1";
                }
            }
            std::cout << "found message ENTER_PIN, ok we continue." << std::endl;

            //if (outputstring.find("ENTER_PIN")!=std::string::npos)
            if (outputstring.find("\"ENTER_PIN\"") != std::string::npos)
            {
                std::cout << "found message ENTER_PIN." << std::endl;

                if (outputstring.find("keypad\":false") != std::string::npos)
                {
                    std::cout << "found keypad:false" << std::endl;

                    //std::cout << "Send command: {\"cmd\": \"SET_PIN\", \"value\": \"" << PINstring << "\"}\n" << std::endl;
                    ws.write(net::buffer(std::string("{\"cmd\": \"SET_PIN\", \"value\": \"" + PINstring + "\"}")));

                }
                else
                {
                    std::cout << "keypad:true" << std::endl;

                    //std::cout << "Send command: {\"cmd\": \"SET_PIN\" }\n" << std::endl;
                    ws.write(net::buffer(std::string("{\"cmd\": \"SET_PIN\"}")));
                }
            }

            outputstring = beast::buffers_to_string(buffer.data());
            std::cout << outputstring << std::endl;
            while (outputstring.find("\"AUTH\"") == std::string::npos)
            {
                buffer.clear();
                ws.read(buffer);
                outputstring = beast::buffers_to_string(buffer.data());
                std::cout << outputstring << std::endl;
                if (outputstring.find("error") != std::string::npos)
                {
                    if (ws.is_open()) {
                        ws.close(websocket::close_code::normal);
                        std::cout << "closed websocketconnection." << std::endl;
                    }
                    return "e1";
                }
            }
            std::string datastring = outputstring;

            //std::cout << datastring << "\n" << std::endl;
            outputstring = datastring.substr(datastring.find("https"));
            datastring = outputstring.substr(0, outputstring.find("\""));
            //std::cout << datastring << std::endl;

            //std::string httpstring = "{\"msg\":\"AUTH\",\"result\":{\"major\":\"http://www.bsi.bund.de/ecard/api/1.1/resultmajor#ok\"},\"url\":\"https://www.autentapp.de/AusweisAuskunft/WebServiceReceiverServlet?refID=123456&mode=json\"}";
            std::string outs;

            outputstring = datastring.substr(datastring.find("https"));
            outs = outputstring.substr(0, outputstring.find("\""));
            //std::cout << outs << std::endl;

            std::string url = outs.substr(8);
            std::cout << "url = " << url << std::endl;
            host = url.substr(0, url.find("/"));
            std::cout << "host = " << host << std::endl;
            target = url.substr(url.find("/"));
            std::cout << "target = " << target << std::endl;

            port = "443"; //=https
            //read https
            ssl::context ctx{ ssl::context::sslv23_client };
            ctx.set_verify_mode(ssl::verify_none);
            ssl::stream<tcp::socket> str{ ioc, ctx };
            auto endpts = resolver.resolve(host, port);
            std::cout << "connect to resulthost." << std::endl;
            net::connect(str.next_layer(), endpts.begin(), endpts.end());
            std::cout << "starting handshake." << std::endl;
            str.handshake(ssl::stream_base::client);
            http::request<http::string_body> srequest{ http::verb::get, target, 11 };
            srequest.set(http::field::host, host);
            http::write(str, srequest);
            http::response<http::string_body> sresp;
            boost::beast::flat_buffer bbuffer;
            http::read(str, bbuffer, sresp);
            //std::cout << "response = " << sresp.body() << std::endl;

            //parse json
            outputstring = readjson(sresp.body().c_str());
            datastring = outputstring;

            //std::cout << "jsonresult = " << datastring << std::endl;
            //generate hash
            outputstring = sha256(datastring);
            //std::cout << "sha256 hash = " << outputstring << std::endl;

            // Close the WebSocket connection
            if (ws.is_open()) {
                ws.close(websocket::close_code::normal);
                std::cout << "closed websocketconnection." << std::endl;
            }
        }
        catch (std::exception const& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
            //return EXIT_FAILURE;
            return "e1";
        }
        //return EXIT_SUCCESS;

        //fclose(stdout);
        std::cout << "outputstring of startworkflow is: " << outputstring << std::endl;
        return outputstring;
    }

} //namespace workflowLibrary