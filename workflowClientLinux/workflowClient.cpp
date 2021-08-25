// workflowClient.cpp : Test of workflowLibrary
// Copyright (C) 2021 buergerservice.org e.V. <KeePerso@buergerservice.org>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)


#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <chrono>
#include <thread>
//for workflowLibraryLinux.a
#include "workflowLibrary.h"




std::wstring strtowstr(const std::string& s)
{
    std::wstring wsTmp(s.begin(), s.end());
    //ws = wsTmp;
    return wsTmp;
}


std::string commandexec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


int main(int argc, char** argv)
{
    std::string PINstring = "";
    std::string outputstring = "";
    std::string workflowoutputstring = "";
    //std::basic_string<TCHAR> textstr;
    //std::locale::global(std::locale("German_germany.UTF-8"));

    //instantiate a new workflowclass
    workflowLibrary::workflow wf;

    std::cout << "\nBuergerservice.org e.V. workflowClientLinux.out test of workflowLibraryLinux.\n\n" << std::endl;
    // Check command line arguments.
    if (argc != 2)
    {
        outputstring = commandexec("ls -l");
        std::cout << outputstring << std::endl;
        std::cerr <<
            "Usage: ./workflowClientLinux.out <PIN>\n" <<
            "Example:\n" <<
            "./workflowClientLinux.out 123456\n";
        return EXIT_FAILURE;
    }
    PINstring = argv[1];
    std::cout << "PIN= " << PINstring << std::endl;

    
    std::cout << "---------------------------------------" << std::endl;
    //if you like test if keypad is true
    std::cout << "starting getkeypad." << std::endl;
    outputstring = wf.getkeypad();
    std::cout << "keypad= " << outputstring << std::endl;
    

    std::cout << "---------------------------------------" << std::endl;
    std::cout << "starting getcertificate." << std::endl;
    wf.getcertificate();
    std::wcout << "parsed certificate as StyledString: " << strtowstr(wf.certificateStyledString) << std::endl;
    // certificate description
    std::wcout << "issuerName: " << strtowstr(wf.issuerName) << std::endl;
    std::wcout << "issuerUrl: " << strtowstr(wf.issuerUrl) << std::endl;
    std::wcout << "purpose: " << strtowstr(wf.purpose) << std::endl;
    std::wcout << "subjectName: " << strtowstr(wf.subjectName) << std::endl;
    std::wcout << "subjectUrl: " << strtowstr(wf.subjectUrl) << std::endl;
    std::wcout << "termsOfUsage: " << strtowstr(wf.termsOfUsage) << std::endl;
    std::wcout << "effectiveDate: " << strtowstr(wf.effectiveDate) << std::endl;
    std::wcout << "expirationDate: " << strtowstr(wf.expirationDate) << std::endl;
    

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "---------------------------------------" << std::endl;
    std::cout << "starting startworkflow." << std::endl;
    //start the workflow of the new workflowclass and show output
    workflowoutputstring = wf.startworkflow(PINstring);

    //std::cout << "starting startworkflow." << std::endl;
    //start the workflow of the new workflowclass and show output
    //workflowoutputstring = wf.startworkflow(PINstring);
    outputstring = workflowoutputstring;
    if (outputstring == "e1")
    {
        std::cout << "ERROR - please check AusweisApp2, cardreaderand Personalausweis!Exiting Plugin." << std::endl;
        return EXIT_FAILURE;
    }
    else if (outputstring == "e2")
    {
        std::cout << "ERROR - please check your Personalausweis! Exiting Plugin." << std::endl;
        return EXIT_FAILURE;
    }
    else if (outputstring == "e3")
    {
        std::cout << "ERROR - please check your cardreader! Exiting Plugin." << std::endl;
        return EXIT_FAILURE;
    }
    else if (outputstring == "e4")
    {
        std::cout << "ERROR - AusweisApp2-version less than 1.22.* please update! Exiting Plugin." << std::endl;
        return EXIT_FAILURE;
    }
    else if (outputstring == "e5")
    {
        std::cout << "Warning - retryCounter of Perso <3, please start a selfauthentication direct with AusweisApp2! Exiting Plugin." << std::endl;
        return EXIT_FAILURE;
    }
    else if (outputstring == "e7")
    {
        std::cout << "ERROR - no cardreader found! Exiting Plugin." << std::endl;
        return EXIT_FAILURE;
    }


    std::cout << "---------------------------------------" << std::endl;
    //read the output of the parsed jsondata
    std::wcout << "parsed jsonresult as StyledString: \n" << strtowstr(wf.personalStyledString) << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    //read the values of single datanames
    std::wcout << "AcademicTitle: " << strtowstr(wf.AcademicTitle) << std::endl;
    std::wcout << "ArtisticName: " << strtowstr(wf.ArtisticName) << std::endl;
    std::wcout << "BirthName: " << strtowstr(wf.BirthName) << std::endl;
    std::wcout << "DateOfBirth: " << strtowstr(wf.DateOfBirth) << std::endl;
    std::wcout << "DocumentType: " << strtowstr(wf.DocumentType) << std::endl;
    std::wcout << "FamilyNames: " << strtowstr(wf.FamilyNames) << std::endl;
    std::wcout << "GivenNames: " << strtowstr(wf.GivenNames) << std::endl;
    std::wcout << "IssuingState: " << strtowstr(wf.IssuingState) << std::endl;
    std::wcout << "Nationality: " << strtowstr(wf.Nationality) << std::endl;
    //personaldata PlaceOfBirth
    std::wcout << "PlaceOfBirth: " << strtowstr(wf.PlaceOfBirth) << std::endl;
    //personaldata PlaceOfResidence
    std::wcout << "City: " << strtowstr(wf.City) << std::endl;
    std::wcout << "Country: " << strtowstr(wf.Country) << std::endl;
    std::wcout << "Street: " << strtowstr(wf.Street) << std::endl;
    std::wcout << "ZipCode: " << strtowstr(wf.ZipCode) << std::endl;
    std::cout << "---------------------------------------\n" << std::endl;
    std::wcout << "keyhash of workflow: \n" << strtowstr(outputstring) << "\n\n" << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    
}
