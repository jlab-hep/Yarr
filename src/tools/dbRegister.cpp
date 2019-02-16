// #################################
// # Author: Arisa Kubota
// # Description: upload to mongoDB
// ################################

#include <iostream>
#include <cstdlib>
#include <string>
#include "json.hpp"

#include "Database.h"

void printHelp();

int main(int argc, char *argv[]){

  std::string registerType = "";

  // register component
	std::string dbConnPath = "";
  
  // register user
  std::string dbUserAccount = "";
  std::string dbUserName = "";
  std::string dbUserIdentity = "";
	std::string dbInstitution = "";
	std::string dbUserCfgPath = "";

  // register address
  std::string dbCite = "";

  int c;
  while ((c = getopt(argc, argv, "hU:C:n:u:i:p:c:")) != -1 ){
    switch (c) {
        case 'h':
            printHelp();
            return 0;
            break;
        case 'U':
            registerType = "User";
            dbUserAccount = std::string(optarg);
            break;
        case 'C':
            registerType = "Component";
            dbConnPath = std::string(optarg);
            break;
        case 'n':
            dbUserName = std::string(optarg);
            break;
        case 'i':
            dbInstitution = std::string(optarg);
            break;
        case 'u':
            dbUserIdentity = std::string(optarg);
            break;
        case 'c':
            dbCite = std::string(optarg);
            break;
        case 'p':
            dbUserCfgPath = std::string(optarg);
            break;
        case '?':
            if(optopt == 'U'||optopt == 'C'){
                std::cerr << "-> Option " << (char)optopt
                          << " requires a parameter! Aborting... " << std::endl;
                return -1;
            } else {
                std::cerr << "-> Unknown parameter: " << (char)optopt << std::endl;
            }
            break;
        default:
            std::cerr << "-> Error while parsing command line parameters!" << std::endl;
            return -1;
    }
  }

  // register user
  if (registerType == "User") {
    std::cout << "Database: Login user: \n\taccount: " << dbUserAccount << std::endl;
    std::string home = getenv("HOME");
    if (dbUserCfgPath == "") {
      dbUserCfgPath = home + "/.yarr/" + dbUserAccount + "_user.json";
    }
    std::ifstream dbUserCfgFile(dbUserCfgPath);
    if (dbUserName == ""||dbInstitution == ""||dbUserIdentity == "") {
      if (dbUserCfgFile) {
        json dbUserCfg = json::parse(dbUserCfgFile);
        if (dbUserName == ""&&!dbUserCfg["userName"].empty()) dbUserName = dbUserCfg["userName"];
        if (dbInstitution == ""&&!dbUserCfg["institution"].empty()) dbInstitution = dbUserCfg["institution"];
        if (dbUserIdentity == ""&&!dbUserCfg["userIdentity"].empty()) dbUserIdentity = dbUserCfg["userIdentity"];
      }
      if (dbUserIdentity == "") dbUserIdentity = "default";
    }
  
    if (dbUserName == ""||dbInstitution == "") {
        std::cerr << "Error: no username/institution given, please specify username/institution under -n/i option!" << std::endl;
        return -1;
    }
  
    std::replace(dbUserName.begin(), dbUserName.end(), ' ', '_');
    std::replace(dbInstitution.begin(), dbInstitution.end(), ' ', '_');
    std::replace(dbUserIdentity.begin(), dbUserIdentity.end(), ' ', '_');

    std::cout << std::endl;
    std::cout << "Database: Register user's information: " << std::endl;
  	std::cout << "\tuser name : " << dbUserName << std::endl;
  	std::cout << "\tinstitution : " << dbInstitution << std::endl;
  	std::cout << "\tuser identity : " << dbUserIdentity << std::endl;
    std::cout << std::endl;

    //setenv("DBUSER", dbUserAccount.c_str(), 1);

  	std::string collection = "user";
  
    Database *database = new Database();
  	database->registerUserInstitution(dbUserName, dbInstitution, dbUserIdentity, dbCite);
    delete database;
  }

  // register user
  if (registerType == "Component") {
    if (dbConnPath == "") {
        std::cerr << "Error: no connectivity config file given, please specify file name under -C option!" << std::endl;
        return -1;
    }

    std::cout << "Database: Register Component:" << std::endl;
	  std::cout << "\tconnecitivity config file : " << dbConnPath << std::endl;
	  std::string collection = "component";

    Database *database = new Database();
    database->setUserInstitution();
	  database->registerFromConnectivity(dbConnPath);

    delete database;
  }

	return 0;
}

void printHelp() {
    std::cout << "Help:" << std::endl;
    std::cout << " -h: Shows this." << std::endl;
    std::cout << " -C <conn.json> : Register component data into database. Provide connectivity config." << std::endl;
    std::cout << " -U <user account> : Register user data into database. Provide user information with options. (Default: -p ~/.yarr/<user account>_user.json)" << std::endl;
    std::cout << "\t -n <user name>" << std::endl;
    std::cout << "\t -i <institution>" << std::endl;
    std::cout << "\t -u <user identity>" << std::endl;
    std::cout << "\t -p <user config path>" << std::endl;
    std::cout << "\t -c <test place> (Default: ~/.yarr/address)" << std::endl;
}