// #################################
// # Author: Eunchong Kim, Arisa Kubota
// # Email: eunchong.kim at cern.ch, arisa.kubota at cern.ch
// # Date : April 2019
// # Project: Local DBHandler for Yarr
// # Description: DBHandler functions
// ################################

#include "DBHandler.h"

#ifdef MONGOCXX_INCLUDE // When use macro "-DMONGOCXX_INCLUDE" in makefile

// bsoncxx
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

// Using bson::builder::stream, an iostream like interface to construct BSON objects.
// And these 'using ~' are greatly useful to reduce coding lines and make it readable.
// Please see example to understand what they are for.
// https://github.com/mongodb/mongo-cxx-driver/blob/r3.2.0-rc1/examples/bsoncxx/builder_stream.cpp
using bsoncxx::builder::stream::document;       // = '{', begining of document
using bsoncxx::builder::stream::finalize;       // = '}'
using bsoncxx::builder::stream::open_document;  // = '{', begining of subdocument
using bsoncxx::builder::stream::close_document; // = '}'
using bsoncxx::builder::stream::open_array;     // = '['
using bsoncxx::builder::stream::close_array;    // = ']'
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;
#endif // End of ifdef MONGOCXX_INCLUDE

DBHandler::DBHandler():
#ifdef MONGOCXX_INCLUDE
client(), db(), 
#endif
m_option(""), m_db_cfg_path(""), m_user_oid_str(""), m_site_oid_str(""),
m_chip_type(""), m_log_dir(""), m_log_path(""), m_cache_path(""), m_cache_dir(""), m_tr_oid_str(""),
m_stage_list(), m_env_list(), m_comp_list(),
m_histo_names(), m_tr_oid_strs(), m_serial_numbers(),
m_db_version(1.0), DB_DEBUG(true), m_log_json(), m_cache_json(), m_conn_json(), counter(0)
{
    if (DB_DEBUG) std::cout << "DBHandler: DBHandler" << std::endl;
}

DBHandler::~DBHandler() {
    if (DB_DEBUG) std::cout << "DBHandler: Exit DBHandler" << std::endl;
}
//*****************************************************************************************************
// Public functions
//
void DBHandler::initialize(std::string i_db_cfg_path, std::string i_option) {
    if (DB_DEBUG) std::cout << "DBHandler: Initializing " << m_option << std::endl;

    if (i_option=="null") {
        std::string message = "Set option in initialize";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }

    m_option = i_option;
    m_db_cfg_path = i_db_cfg_path;

    int now = std::time(NULL);

    json db_json = this->checkDBCfg(m_db_cfg_path);
    std::string host_ip = "mongodb://"+std::string(db_json["hostIp"])+":"+std::string(db_json["hostPort"]);
    std::string db_name = db_json["dbName"];
    m_cache_dir = db_json["cachePath"];

    if (m_stage_list.size()==0&&m_env_list.size()==0&&m_comp_list.size()==0) {
        for(auto s_tmp: db_json["stage"]) m_stage_list.push_back(s_tmp);
        for(auto s_tmp: db_json["environment"]) m_env_list.push_back(s_tmp);
        for(auto s_tmp: db_json["component"]) m_comp_list.push_back(s_tmp);
    }

    // initialize for registeration
    if (m_option=="db"||m_option=="register") {
#ifdef MONGOCXX_INCLUDE
        try {
            mongocxx::instance inst{}; // This should be done only once.
        } catch (const mongocxx::exception & e){
            if (DB_DEBUG) std::cout << "Already initialized MongoDB server" << "\n\twhat(): " << e.what() << std::endl;
        }
        try {
            client = mongocxx::client{mongocxx::uri{host_ip}};
            db = client[db_name]; 
            auto doc_value = make_document(kvp("hash", 1), kvp("_id", 1));
            db["fs.files"].create_index(doc_value.view()); 
        } catch (const mongocxx::exception & e){
            std::string message = "Could not access to MongoDB server: "+host_ip+"\n\twhat(): "+e.what();
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
#else
        std::string message = "Not found MongoDB C++ Driver!";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
#endif
    }
    if (m_option=="scan") {
        std::string cmd = "rm "+m_cache_dir+"/tmp/localdb/*.json";
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem removing json files in "+m_cache_dir+"/tmp/localdb";
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
    }
    if (m_option=="scan"||m_option=="dcs") m_log_dir = m_cache_dir+"/tmp/localdb/cache/"+std::to_string(now);
    else m_log_dir = m_cache_dir+"/tmp/localdb/log";//TODO

    m_log_path = m_log_dir + "/cacheLog.json";
    this->mkdir(m_log_dir);
    m_log_json["version"] = m_db_version;
    m_log_json["logPath"] = m_log_dir;
    m_log_json["dbOption"] = m_option;
    this->writeJson("status", "running", m_log_path, m_log_json);
    this->cacheDBCfg(); 
}

void DBHandler::alert(std::string i_function, std::string i_message, std::string i_type) {
    if (DB_DEBUG) std::cout << "DBHandler: Alert '" << i_type << "'" << std::endl;

    std::string alert_message;
    if (i_type=="error") {
        alert_message = "#DB ERROR#";
        m_log_json["errormessage"] = i_message;
        this->writeJson("status", "failure", m_log_path, m_log_json);
        if (m_option=="db") {
            m_cache_json["errormessage"] = i_message;
            this->writeJson("status", "failure", m_cache_path, m_cache_json);
        }
    } else if (i_type=="warning") {
        alert_message = "#DB WARNING#";
    } else if (i_type=="status") {
        alert_message = "#DB STATUS#";
    }
    std::cerr << alert_message << " " << i_message << std::endl;
    std::time_t now = std::time(NULL);
    struct tm *lt = std::localtime(&now);
    char tmp[20];
    strftime(tmp, 20, "%Y%m%d", lt);
    std::string timestamp=tmp;
    std::string log_path = m_cache_dir+"/var/log/localdb/"+timestamp+"_error.log";
    std::ofstream file_ofs(log_path, std::ios::app);
    strftime(tmp, 20, "%F_%H:%M:%S", lt);
    timestamp=tmp;
    file_ofs << timestamp << " " << alert_message << " [" << m_option << "] " << i_function << std::endl;
    file_ofs << "Message: " << i_message << std::endl;
    file_ofs << "Log: " << m_log_path << std::endl;
    file_ofs << "Cache: " << m_cache_path << std::endl;
    file_ofs << "--------------------" << std::endl;

    if (i_type=="error") std::abort();

    return;
}

int DBHandler::checkLibrary() {
    if (DB_DEBUG) std::cout << "DBHandler: Check Mongocxx library" << std::endl;
#ifdef MONGOCXX_INCLUDE
    std::string message = "Found MongoDB C++ Driver!";
    std::string function = __PRETTY_FUNCTION__;
    this->alert(function, message, "status");
    return 0;
#else
    std::string message = "Not found MongoDB C++ Driver!";
    std::string function = __PRETTY_FUNCTION__;
    this->alert(function, message, "warning");
    return 1;
#endif
}

int DBHandler::checkConnection(std::string i_db_cfg_path) {
    if (DB_DEBUG) std::cout << "DBHandler: Check LocalDB server connection" << std::endl;
#ifdef MONGOCXX_INCLUDE
    json db_json = this->checkDBCfg(i_db_cfg_path);
    std::string db_cfg_path = i_db_cfg_path;
    std::string host_ip = "mongodb://"+std::string(db_json["hostIp"])+":"+std::string(db_json["hostPort"]);
    //mongocxx::instance inst{};
    try {
        client = mongocxx::client{mongocxx::uri{host_ip}};
        db = client["localdb"]; // Database name is 'localdb'
        auto doc_value = document{} << finalize;
        db["component"].find(doc_value.view()); 
    } catch (const mongocxx::exception & e){
        std::string message = "Could not access to MongoDB server: "+host_ip+"\n\twhat(): "+e.what();
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message, "warning");
        return 1;
    }
    return 0;
#else
    std::string message = "Not found MongoDB C++ Driver!";
    std::string function = __PRETTY_FUNCTION__;
    this->alert(function, message, "warning");
    return 1;
#endif
}

int DBHandler::checkModuleList() { 
    if (DB_DEBUG) std::cout << "DBHandler: Check Module List" << std::endl;

#ifdef MONGOCXX_INCLUDE
    std::string mod_list_path = m_cache_dir+"/var/lib/localdb/modules.csv";
    std::ofstream list_ofs(mod_list_path, std::ios::out);

    auto doc_value = make_document(kvp("componentType", "Module"));
    mongocxx::cursor cursor = db["component"].find(doc_value.view());
    for (auto doc : cursor) {
        std::string mo_serial_number = doc["serialNumber"].get_utf8().value.to_string();
        list_ofs << mo_serial_number << ",";

        std::string oid_str = doc["_id"].get_oid().value.to_string();
        doc_value = make_document(kvp("parent", oid_str));
        mongocxx::cursor cursor_cpr = db["childParentRelation"].find(doc_value.view());
        for (auto child : cursor_cpr) {
            std::string ch_oid_str = child["child"].get_utf8().value.to_string();

            std::string chip_id = this->getValue("component", "_id", ch_oid_str, "oid", "chipId", "int");
            std::string ch_serial_number = this->getValue("component", "_id", ch_oid_str, "oid", "serialNumber");
            list_ofs << chip_id << "," << ch_serial_number << ",";
        }
        list_ofs << std::endl;
    }
    list_ofs.close();
#endif
    return 0;
}

// public
void DBHandler::setUser(std::string i_user_path) {
    // Set UserID from DB
    if (DB_DEBUG) std::cout << "DBHandler: Set user: " << i_user_path << std::endl;

    std::string user_name, institution, user_identity;
    if (i_user_path=="") {
        user_name = getenv("USER");
        institution = getenv("HOSTNAME");
        user_identity = "default";
    } else {
        json user_json = this->checkUserCfg(i_user_path);
        user_name = user_json["userName"];
        institution = user_json["institution"];
        user_identity = user_json["userIdentity"];
    }
    std::cout << "DBHandler: User Information \n\tUser name: " << user_name << "\n\tInstitution: " << institution << "\n\tUser identity: " << user_identity << std::endl;

#ifdef MONGOCXX_INCLUDE
    if (m_option=="db"||m_option=="register") {
        m_user_oid_str = this->registerUser(user_name, institution, user_identity);
    }
#endif
    this->cacheUser(i_user_path);

    return;
}

void DBHandler::setSite(std::string i_address_path) {
    // Set UserID from DB
    if (DB_DEBUG) std::cout << "DBHandler: Set site: " << i_address_path << std::endl;

    // Set MAC address
    if (DB_DEBUG) std::cout << "DBHandler: Set address" << std::endl;
    json address_json = this->checkAddressCfg(i_address_path);
    std::string address = address_json["macAddress"];
    std::string hostname = address_json["hostname"];
    std::string site = address_json["institution"];
    std::cout << "DBHandler: MAC Address: " << address << std::endl;

#ifdef MONGOCXX_INCLUDE
    if (m_option=="db"||m_option=="register") {
        m_site_oid_str = this->registerSite(address, hostname, site);
    }
#endif
    this->cacheSite(i_address_path);

    return;
}


void DBHandler::setConnCfg(std::vector<std::string> i_conn_paths) {
    if (DB_DEBUG) std::cout << "DBHandler: Set connectivity config" << std::endl;

    if (m_option=="register") this->checkModuleList(); //TODO should be checked

    for (auto conn_path : i_conn_paths) {
        if (DB_DEBUG) std::cout << "\tDBHandler: setting connectivity config file: " << conn_path << std::endl;
        json conn_json = this->checkConnCfg(conn_path);
        if (m_option=="db") m_conn_json["connectivity"].push_back(conn_json);
        m_chip_type = conn_json["chipType"];
        if (m_chip_type == "FEI4B") m_chip_type = "FE-I4B";
    }
#ifdef MONGOCXX_INCLUDE
    if (m_option=="register") this->registerConnCfg(i_conn_paths);
#endif
    this->cacheConnCfg(i_conn_paths);
}

void DBHandler::setDCSCfg(std::string i_dcs_path, std::string i_tr_path) {
    if (DB_DEBUG) std::cout << "DBHandler: Set DCS config: " << i_dcs_path << std::endl;

    json tr_json = this->toJson(i_tr_path);
    if (tr_json["id"].empty()) {
        this->checkEmpty(tr_json["startTime"].empty(), "testRun.startTime", i_tr_path);
        this->checkEmpty(tr_json["serialNumber"].empty(), "testRun.serialNumber", i_tr_path);
    }
    std::string tr_oid_str = "";
    std::string serial_number = "";
    int timestamp = -1;
    if (!tr_json["id"].empty()) tr_oid_str = tr_json["id"];
    if (!tr_json["serialNumber"].empty()) serial_number = tr_json["serialNumber"];
    if (!tr_json["startTime"].empty()) timestamp = tr_json["startTime"];
    this->getTestRunData(tr_oid_str, serial_number, timestamp);
 
    json dcs_json = this->toJson(i_dcs_path);
    if (dcs_json["environments"].empty()) return;
    json env_json = dcs_json["environments"];

    for (int i=0; i<(int)env_json.size(); i++) {
        std::string num_str = std::to_string(i);
        this->checkEmpty(env_json[i]["status"].empty(), "environments."+num_str+".status", i_dcs_path, "Set enabled/disabled to register.");
        if (env_json[i]["status"]!="enabled") continue;

        this->checkDCSCfg(i_dcs_path, num_str, env_json[i]);
        if (!env_json[i]["path"].empty()) {
            int j_num = env_json[i]["num"];
            std::string log_path = "";
            if (m_option=="dcs") log_path = env_json[i]["path"];
            else if (m_option=="db") log_path = std::string(m_cache_json["logPath"])+"/"+std::string(env_json[i]["path"]);
            this->checkDCSLog(log_path, i_dcs_path, env_json[i]["key"], j_num); 
        } else {
            this->checkNumber(env_json[i]["value"].is_number(), "environments."+num_str+".value", i_dcs_path);
        }
    }
    this->cacheDCSCfg(i_dcs_path, i_tr_path);

    return;
}

void DBHandler::setTestRunStart(std::string i_test_type, std::vector<std::string> i_conn_paths, int i_run_number, int i_target_charge, int i_target_tot, int i_timestamp, std::string i_command) {
    if (DB_DEBUG) std::cout << "DBHandler: Write Test Run (start)" << std::endl;

    for (int i=0; i<(int)i_conn_paths.size(); i++) {
        std::string conn_path = i_conn_paths[i];
        json conn_json = this->toJson(conn_path);
        std::string mo_serial_number;
        std::string chip_type = conn_json["chipType"];
        if (conn_json["module"]["serialNumber"].empty()) mo_serial_number = "DUMMY_" + std::to_string(i);
        else mo_serial_number = conn_json["module"]["serialNumber"];
#ifdef MONGOCXX_INCLUDE
        if (m_option=="db"||m_option=="register") {
            std::string tr_oid_str = this->registerTestRun(i_test_type, i_run_number, i_target_charge, i_target_tot, i_timestamp, mo_serial_number, "start");
            // command
            this->addValue(tr_oid_str, "testRun", "command", i_command);
            // stage
            if (!conn_json["stage"].empty()) {
                std::string stage = conn_json["stage"];
                this->addValue(tr_oid_str, "testRun", "stage", stage);
            }
            // dummy
            if (conn_json["dummy"]) {
                this->addValue(tr_oid_str, "testRun", "dummy", "true", "bool");
            }
            m_tr_oid_strs.push_back(tr_oid_str);
            this->registerComponentTestRun(conn_path, tr_oid_str, i_test_type, i_run_number);
            m_serial_numbers.push_back(mo_serial_number);
        }
#endif
        if (m_option=="scan") {
            std::time_t now = std::time(NULL);
            struct tm *lt = std::localtime(&now);
            char tmp[20];
            strftime(tmp, 20, "%Y%m", lt);
            std::string ts=tmp;
            std::string log_path = m_cache_dir+"/var/lib/localdb/"+ts+"_scan.csv";
            std::ofstream log_file_ofs(log_path, std::ios::app);
            strftime(tmp, 20, "%F_%H:%M:%S", lt);
            ts=tmp;
            log_file_ofs << ts << "," << mo_serial_number << "," << i_timestamp << "," << i_run_number << "," << i_test_type << std::endl;
            log_file_ofs.close();

            json tr_json;
            tr_json["startTime"] = i_timestamp;
            tr_json["serialNumber"] = mo_serial_number;
            tr_json["runNumber"] = i_run_number;
            tr_json["testType"] = i_test_type;

            std::string tmp_path = m_cache_dir+"/tmp/localdb/"+mo_serial_number+"_scan.json";
            std::ofstream tmp_file_ofs(tmp_path);
            tmp_file_ofs << std::setw(4) << tr_json;
            tmp_file_ofs.close();
        }
    }
    this->cacheTestRun(i_test_type, i_run_number, i_target_charge, i_target_tot, i_timestamp, -1, i_command);

    return;
}

void DBHandler::setTestRunFinish(std::string i_test_type, std::vector<std::string> i_conn_paths, int i_run_number, int i_target_charge, int i_target_tot, int i_timestamp, std::string i_command) {
    if (DB_DEBUG) std::cout << "DBHandler: Write Test Run (finish)" << std::endl;

    std::sort(m_histo_names.begin(), m_histo_names.end());
    m_histo_names.erase(std::unique(m_histo_names.begin(), m_histo_names.end()), m_histo_names.end());
#ifdef MONGOCXX_INCLUDE
    if (m_option=="db"||m_option=="register") {
        for (auto tr_oid_str : m_tr_oid_strs) {
            this->registerTestRun(i_test_type, i_run_number, i_target_charge, i_target_tot, i_timestamp, "", "finish", tr_oid_str);
        }
    }
#endif
    this->cacheTestRun(i_test_type, i_run_number, i_target_charge, i_target_tot, -1, i_timestamp, i_command);
    
    return;
}

void DBHandler::setConfig(int i_tx_channel, int i_rx_channel, std::string i_file_path, std::string i_filename, std::string i_title, std::string i_collection, std::string i_serial_number) {
    if (DB_DEBUG) std::cout << "DBHandler: Write Config Json: " << i_file_path << std::endl;

    std::ifstream file_ifs(i_file_path);
    if (!file_ifs) return;
    file_ifs.close();

    std::string serial_number;
    if (i_serial_number!="") {
        serial_number = i_serial_number;
    } else {
        for (auto conn_json : m_conn_json["connectivity"]) {
            for (auto chip_json : conn_json["chips"]) {
                if (chip_json["tx"]==i_tx_channel&&chip_json["rx"]==i_rx_channel) serial_number = chip_json["serialNumber"];
            }
        }
    }
#ifdef MONGOCXX_INCLUDE
    if (m_option=="db"||m_option=="register") this->registerConfig(serial_number, i_file_path, i_filename, i_title, i_collection);
#endif
    this->cacheConfig(serial_number, i_file_path, i_filename, i_title, i_collection);

    return;
}

//void DBHandler::setAttachment(std::string i_serial_number, std::string i_file_path, std::string i_histo_name) {
void DBHandler::setAttachment(int i_tx_channel, int i_rx_channel, std::string i_file_path, std::string i_histo_name, std::string i_serial_number) {
    if (DB_DEBUG) std::cout << "DBHandler: Write Attachment: " << i_file_path << std::endl;

    std::ifstream file_ifs(i_file_path);
    if (!file_ifs) return;
    file_ifs.close();

    std::string serial_number;
    if (i_serial_number!="") {
        serial_number = i_serial_number;
    } else {
        for (auto conn_json : m_conn_json["connectivity"]) {
            for (auto chip_json : conn_json["chips"]) {
                if (chip_json["tx"]==i_tx_channel&&chip_json["rx"]==i_rx_channel) serial_number = chip_json["serialNumber"];
            }
        }
    }
#ifdef MONGOCXX_INCLUDE
    if (m_option=="db"||m_option=="register") this->registerAttachment(serial_number, i_file_path, i_histo_name);
#endif
    this->cacheAttachment(serial_number, i_file_path, i_histo_name);

    m_histo_names.push_back(i_histo_name);

    return;
}

void DBHandler::setCache(std::string i_cache_dir) {
    if (DB_DEBUG) std::cout << "DBHandler: Write cache data: " << i_cache_dir << std::endl;

#ifdef MONGOCXX_INCLUDE
    m_cache_path = i_cache_dir+"/cacheLog.json";
    m_cache_json = this->toJson(m_cache_path);
    this->checkEmpty(m_cache_json["dbOption"].empty(), "dbOption", m_cache_path, "");
    m_log_json["cachePath"] = m_cache_json["logPath"];

    if (m_cache_json["dbOption"]=="scan") this->writeScan(i_cache_dir);
    if (m_cache_json["dbOption"]=="dcs") this->writeDCS(i_cache_dir);
#else
    std::string message = "Not found MongoDB C++ Driver!";
    std::string function = __PRETTY_FUNCTION__;
    this->alert(function, message);
#endif
    return;
}

void DBHandler::cleanUp(std::string i_dir) {
    if (m_option=="scan") {
        int now = std::time(NULL);
        std::string cmd = "mv "+m_log_dir+" "+m_cache_dir+"/var/cache/localdb/"+std::to_string(now);
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem moving directory "+m_log_dir+" to "+m_cache_dir+"/var/cache/localdb/"+std::to_string(now);
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        m_log_dir = m_cache_dir+"/var/cache/localdb/"+std::to_string(now);
        m_log_json["logPath"] = m_log_dir;
        m_log_path = m_log_dir + "/cacheLog.json";
        this->writeJson("status", "waiting", m_log_path, m_log_json);
    } else if (m_option=="dcs") {
        int now = std::time(NULL);
        std::string cmd = "mv "+m_log_dir+" "+m_cache_dir+"/var/cache/localdb/"+std::to_string(now);
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem moving directory "+m_log_dir+" to "+m_cache_dir+"/var/cache/localdb/"+std::to_string(now);
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        m_log_dir = m_cache_dir+"/var/cache/localdb/"+std::to_string(now);
        m_log_json["logPath"] = m_log_dir;
        m_log_path = m_log_dir + "/cacheLog.json";
        this->writeJson("status", "waiting", m_log_path, m_log_json);
    }
#ifdef MONGOCXX_INCLUDE
    else if (m_option=="db") {
        if (m_cache_json["dbOption"]=="scan") {
            for (auto serial_number : m_serial_numbers) {
                int start_time = m_cache_json["startTime"];
                this->getTestRunData("", serial_number, start_time);
                if (m_tr_oid_str=="") {
                    m_log_json["errormessage"] = "Failed to upload, try again";
                    m_cache_json["errormessage"] = "Failed to upload, try again";
                    this->writeJson("status", "failure", m_log_path, m_log_json);
                    this->writeJson("status", "failure", m_cache_path, m_cache_json);
                }
            }
        } else if (m_cache_json["dbOption"]=="dcs") {
            auto doc_value = make_document(kvp("_id", bsoncxx::oid(m_tr_oid_str)));
            auto result = db["testRun"].find_one(doc_value.view());
            if (result->view()["environment"].get_utf8().value.to_string()=="...") {
                m_log_json["errormessage"] = "Failed to upload, try again";
                m_cache_json["errormessage"] = "Failed to upload, try again";
                this->writeJson("status", "failure", m_log_path, m_log_json);
                this->writeJson("status", "failure", m_cache_path, m_cache_json);
            }
        }
        if (m_log_json["status"]!="failure") this->writeJson("status", "done", m_log_path, m_log_json);
        if (m_cache_json["status"]!="failure") {
            this->writeJson("status", "done", m_cache_path, m_cache_json);
            int now = std::time(NULL);
            std::string cmd = "mv "+i_dir+" "+m_cache_dir+"/tmp/localdb/done/"+std::to_string(now);
            if (system(cmd.c_str()) < 0) {
                std::string message = "Problem moving directory "+i_dir+" into "+m_cache_dir+"/tmp/localdb/done/"+std::to_string(now);
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message); return;
            }
        }
    }
#endif
    // initialize
    m_option = "";
    m_db_cfg_path = "";
    m_user_oid_str = "";
    m_site_oid_str = "";
    m_chip_type = "";
    m_log_dir = "";
    m_log_path = "";
    m_cache_path = "";
    m_cache_dir = "";
    m_tr_oid_str = "";
    m_stage_list.clear();
    m_env_list.clear();
    m_comp_list.clear();
    m_histo_names.clear();
    m_tr_oid_strs.clear();
    m_serial_numbers.clear();
    m_log_json = NULL;
    m_cache_json = NULL;
    m_conn_json = NULL;
    counter = 0;
}

void DBHandler::getData(std::string i_info_file_path, std::string i_get_type) {
    if (DB_DEBUG) std::cout << "DBHandler: Get Data" << std::endl;
    
#ifdef MONGOCXX_INCLUDE
    json info_json;
    if (i_info_file_path!="") info_json = toJson(i_info_file_path);
    if (i_get_type == "testRunLog") this->getTestRunLog(info_json);
    if (i_get_type == "testRunId") this->getTestRunId(info_json);
    if (i_get_type == "scan") this->getScan(info_json);
    if (i_get_type == "config") this->getConfigData(info_json);
    if (i_get_type == "userId") this->getUserId(info_json);
    if (i_get_type == "dat") this->getDatData(info_json);
#else
    std::string message = "Not found MongoDB C++ Driver!";
    std::string function = __PRETTY_FUNCTION__;
    this->alert(function, message);
#endif

    return;
}

//*****************************************************************************************************
// Protected fuctions
//
#ifdef MONGOCXX_INCLUDE
std::string DBHandler::getValue(std::string i_collection_name, std::string i_member_key, std::string i_member_value, std::string i_member_bson_type, std::string i_key, std::string i_bson_type){
    if (DB_DEBUG) std::cout << "\tDBHandler: Get value of key: '" << i_key << "' from: '" << i_collection_name << "', {'" << i_member_key << ": '" << i_member_value << "'}" << std::endl;
    mongocxx::collection collection = db[i_collection_name];
    auto query = document{};
    if (i_member_bson_type == "oid") query << i_member_key << bsoncxx::oid(i_member_value);
    else query << i_member_key << i_member_value; 
    auto result = collection.find_one(query.view());
    if(result) {
        if (i_bson_type == "oid") {
            bsoncxx::document::element element = result->view()["_id"];
            return element.get_oid().value.to_string();
        }
        else if (i_bson_type == "sys_cts") {
            bsoncxx::document::element element = result->view()["sys"]["cts"];
            std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(element.get_date().value);
            std::time_t t = s.count();
            std::tm time_tm = *std::localtime(&t);
            char buffer[80];
            strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",&time_tm);
            std::string str(buffer);
            return str;
        }
        else if (i_bson_type == "sys_rev") {
            bsoncxx::document::element element = result->view()["sys"]["rev"];
            return std::to_string(element.get_int32().value);
        }
        else if (i_bson_type == "sys_mts") {
            bsoncxx::document::element element = result->view()["sys"]["mts"];
            std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(element.get_date().value);
            std::time_t t = s.count();
            std::tm time_tm = *std::localtime(&t);
            char buffer[80];
            strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",&time_tm);
            std::string str(buffer);
            return str;
        }
        else if (i_bson_type == "int") {
            bsoncxx::document::element element = result->view()[i_key];
            return std::to_string(element.get_int32().value);
        }
        else {
            bsoncxx::document::element element = result->view()[i_key];
            if (!element) return "ERROR";
            return element.get_utf8().value.to_string();
        }
    }
    else {
        std::string message = "Cannot find '" + i_key + "' from member '" + i_member_key + ": " + i_member_value + "' in collection name: '" + i_collection_name + "'";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return "ERROR";
    }
}

std::string DBHandler::getComponent(std::string i_serial_number) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Get component data: 'serialnumber':" << i_serial_number << std::endl;
    std::string oid_str = "";
    bsoncxx::document::value query_doc = document{} <<  
        "serialNumber"  << i_serial_number <<
    finalize;
    auto maybe_result = db["component"].find_one(query_doc.view());
    if (maybe_result) {
        oid_str = maybe_result->view()["_id"].get_oid().value.to_string();
    }
    return oid_str;
}

// Register Function
std::string DBHandler::registerUser(std::string i_user_name, std::string i_institution, std::string i_user_identity) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register user \n\tUser name: " << i_user_name << "\n\tInstitution: " << i_institution << "\n\tUser identity: " << i_user_identity << std::endl;
    mongocxx::collection collection = db["user"];

    auto doc_value = document{} <<
        "userName"     << i_user_name <<        
        "institution"  << i_institution << 
        "userIdentity" << i_user_identity << 
    finalize;
    auto maybe_result = collection.find_one(doc_value.view());
    if (maybe_result) return maybe_result->view()["_id"].get_oid().value.to_string();

    doc_value = document{} <<  
        "sys"           << open_document << close_document <<
        "userName"      << i_user_name <<
        "userIdentity"  << i_user_identity <<
        "institution"   << i_institution <<
        "userType"      << "readWrite" <<
        "dbVersion"     << -1 <<
    finalize;
    auto oid = collection.insert_one(doc_value.view())->inserted_id().get_oid().value;
    this->addSys(oid.to_string(), "user");
    this->addVersion("user", "_id", oid.to_string(), "oid");

    return oid.to_string();
}

std::string DBHandler::registerSite(std::string i_address, std::string i_hostname, std::string i_site) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register site \n\tAddress: " << i_address << "\n\tName: " << i_hostname << "\n\tInstitution: " << i_site << std::endl;
    mongocxx::collection collection = db["institution"];
    // Set MAC address
    auto doc_value = document{} << 
        "address"     << i_address << 
        "institution" << i_site << 
    finalize;
    auto maybe_result = collection.find_one(doc_value.view());
    if (maybe_result) return maybe_result->view()["_id"].get_oid().value.to_string();

    doc_value = document{} <<
        "sys"         << open_document << close_document <<
        "name"        << i_hostname <<
        "institution" << i_site <<
        "address"     << i_address <<
        "dbVersion"     << -1 <<
    finalize;
    bsoncxx::oid oid = collection.insert_one(doc_value.view())->inserted_id().get_oid().value;
    this->addSys(oid.to_string(), "institution");
    this->addVersion("institution", "_id", oid.to_string(), "oid");

    return oid.to_string();
}

// Will be deleted // TODO enable to register component in viewer application
void DBHandler::registerConnCfg(std::vector<std::string> i_conn_paths) {
    if (DB_DEBUG) std::cout << "DBHandler: Register Component" << std::endl;
    for (auto conn_path : i_conn_paths) {
        if (DB_DEBUG) std::cout << "\tDBHandler: Register from connectivity: " << conn_path << std::endl;

        json conn_json = this->toJson(conn_path);
        std::string mo_serial_number = conn_json["module"]["serialNumber"];

        /// Confirmation
        bool module_is_exist = false;
        bool chip_is_exist = false;
        bool cpr_is_fine = true;
        // Module
        std::string mo_oid_str = this->getComponent(mo_serial_number);
        if (mo_oid_str!="") module_is_exist = true;
        // Chip
        int chips = 0;
        for (unsigned i=0; i<conn_json["chips"].size(); i++) {
            std::string ch_serial_number = conn_json["chips"][i]["serialNumber"];
            std::string chip_oid_str = this->getComponent(ch_serial_number);
            if (chip_oid_str!="") {
                chip_is_exist = true;
                auto doc_value = document{} <<
                    "parent" << mo_oid_str <<
                    "child" << chip_oid_str <<
                    "status" << "active" <<
                finalize;
                auto maybe_result = db["childParentRelation"].find_one(doc_value.view());
                if (!maybe_result) cpr_is_fine = false;
            }
            chips++;
        }
        if (module_is_exist&&!chip_is_exist) {
            std::string message = "There are registered module in connectivity : "+conn_path;
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        } else if (!module_is_exist&&chip_is_exist) {
            std::string message = "There are registered chips in connectivity : "+conn_path;
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        } else if (!cpr_is_fine) {
            std::string message = "There are wrong relationship between module and chips in connectivity : "+conn_path;
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        } else if (module_is_exist&&chip_is_exist&&cpr_is_fine) {
            return;
        }
     
        // Register module component
        std::string mo_component_type = conn_json["module"]["componentType"];
        mo_oid_str = this->registerComponent(mo_serial_number, mo_component_type, -1, chips);
    
        for (unsigned i=0; i<conn_json["chips"].size(); i++) {
            std::string ch_serial_number  = conn_json["chips"][i]["serialNumber"];
            std::string ch_component_type = conn_json["chips"][i]["componentType"];
            int chip_id = conn_json["chips"][i]["chipId"];
            // Register chip component
            std::string ch_oid_str = this->registerComponent(ch_serial_number, ch_component_type, chip_id, -1);
            this->registerChildParentRelation(mo_oid_str, ch_oid_str, chip_id);
    
            json chip_json;
            chip_json["serialNumber"] = ch_serial_number;
            chip_json["componentType"] = ch_component_type;
            chip_json["chipId"] = chip_id;
        }
    }
}

std::string DBHandler::registerComponent(std::string i_serial_number, std::string i_component_type, int i_chip_id, int i_chips) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register Component: " << i_serial_number << std::endl;
    mongocxx::collection collection = db["component"];

    auto doc_value = document{} <<  
        "serialNumber"  << i_serial_number <<
    finalize;
    auto maybe_result = collection.find_one(doc_value.view());
    if (maybe_result) return maybe_result->view()["_id"].get_oid().value.to_string();

    doc_value = document{} <<  
        "sys"           << open_document << close_document <<
        "serialNumber"  << i_serial_number <<
        "chipType"      << m_chip_type <<
        "componentType" << i_component_type <<
        "children"      << i_chips <<
        "chipId"        << i_chip_id <<
        "dbVersion"     << -1 <<
        "address"       << "..." <<
        "user_id"       << "..." <<
    finalize;
    auto oid = collection.insert_one(doc_value.view())->inserted_id().get_oid().value;
    std::string oid_str = oid.to_string();
    this->addSys(oid_str, "component");
    this->addUser("component", oid_str);
    this->addVersion("component", "_id", oid_str, "oid");

    return oid_str;
}

void DBHandler::registerChildParentRelation(std::string i_parent_oid_str, std::string i_child_oid_str, int i_chip_id) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register childParentRelation." << std::endl;
    mongocxx::collection collection = db["childParentRelation"];

    auto doc_value = document{} <<  
        "parent" << i_parent_oid_str <<
        "child"  << i_child_oid_str <<
        "status" << "active" <<
    finalize;
    auto maybe_result = collection.find_one(doc_value.view());
    if (maybe_result) return;

    doc_value = document{} <<  
        "sys"       << open_document << close_document <<
        "parent"    << i_parent_oid_str <<
        "child"     << i_child_oid_str <<
        "chipId"    << i_chip_id <<
        "status"    << "active" <<
        "dbVersion" << -1 <<
    finalize;

    std::string oid_str = collection.insert_one(doc_value.view())->inserted_id().get_oid().value.to_string();
    this->addSys(oid_str, "childParentRelation");
    this->addVersion("childParentRelation", "_id", oid_str, "oid");

    return;
}

void DBHandler::registerConfig(std::string i_serial_number, std::string i_file_path, std::string i_filename, std::string i_title, std::string i_collection) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register Config Json: " << i_file_path << std::endl;

    std::string ctr_oid_str = "";
    if (i_serial_number!="") {
        auto doc_value = document{} << 
            "serialNumber" << i_serial_number << 
        finalize;
        auto maybe_result = db["component"].find_one(doc_value.view());
        std::string cmp_oid_str;
        if (maybe_result) {
            cmp_oid_str = maybe_result->view()["_id"].get_oid().value.to_string();
        } else {
            cmp_oid_str = i_serial_number;
        }
        std::string oid_str;
        for (auto tr_oid_str : m_tr_oid_strs) {
            doc_value = document{} <<
                "component" << cmp_oid_str <<
                "testRun"   << tr_oid_str  <<
            finalize;
            auto result = db["componentTestRun"].find_one(doc_value.view());
            if (result) {
                ctr_oid_str = result->view()["_id"].get_oid().value.to_string();
                break;
            }
        }
    }

    std::string cfg_oid_str;
    if (i_collection=="testRun") {
        cfg_oid_str = this->registerJsonCode(i_file_path, i_filename+".json", i_title, "gj");
        for (auto tr_oid_str : m_tr_oid_strs) {
            if (tr_oid_str=="") continue;
            this->addValue(tr_oid_str, i_collection, i_title, cfg_oid_str);
        }
    }
    if (i_collection == "componentTestRun") {
        if (ctr_oid_str=="") return;
        cfg_oid_str = this->registerJsonCode(i_file_path, i_title+".json", i_title, "gj");
        this->addValue(ctr_oid_str, i_collection, i_filename, cfg_oid_str);
    }
    return;
}

void DBHandler::registerAttachment(std::string i_serial_number, std::string i_file_path, std::string i_histo_name) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register Attachment: " << i_file_path << std::endl;

    std::string ctr_oid_str = "";
    if (i_serial_number!="") {
        auto doc_value = document{} << 
            "serialNumber" << i_serial_number <<
        finalize; 
        auto maybe_result = db["component"].find_one(doc_value.view());
        std::string cmp_oid_str;
        if (maybe_result) {
            cmp_oid_str = maybe_result->view()["_id"].get_oid().value.to_string();
        } else {
            cmp_oid_str = i_serial_number;
        }
        std::string oid_str;
        for (auto tr_oid_str : m_tr_oid_strs) {
            doc_value = document{} <<
                "component" << cmp_oid_str <<
                "testRun" << tr_oid_str <<
            finalize;
            auto result = db["componentTestRun"].find_one(doc_value.view());
            if (result) {
                ctr_oid_str = result->view()["_id"].get_oid().value.to_string();
                break;
            }
        }
    }
    if (ctr_oid_str=="") return;

    std::string file_path = i_file_path;
    std::ifstream file_ifs(file_path);
    if (file_ifs) {
        std::string oid_str = this->writeGridFsFile(file_path, i_histo_name + ".dat");
        this->addAttachment(ctr_oid_str, "componentTestRun", oid_str, i_histo_name, "describe", "dat", i_histo_name+".dat");
    }
}

void DBHandler::registerDCS(std::string i_dcs_path, std::string i_tr_path) {
    if (DB_DEBUG) std::cout << "DBHandler: Register Environment: " << i_dcs_path << std::endl;

    json dcs_json = toJson(i_dcs_path);
    json tr_json = toJson(i_tr_path);

    mongocxx::collection collection = db["environment"];

    // register the environmental key and description
    if (m_tr_oid_str=="") {
        std::string message = "Not found relational test run data in DB";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    auto doc_value = make_document(kvp("_id", bsoncxx::oid(m_tr_oid_str)));
    auto result = db["testRun"].find_one(doc_value.view());
    if (result->view()["environment"].get_utf8().value.to_string()!="...") {
        std::string message = "Already registered dcs data to this test run data in DB";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    if (dcs_json["environments"].empty()) return;
    json env_json = dcs_json["environments"];
    std::vector<std::string> env_keys, descriptions, env_modes, env_paths;
    std::vector<float> env_settings, env_vals, env_margins;
    std::vector<int> env_nums;
    for (auto j: env_json) {
        if (j["status"]!="enabled") continue;

        env_keys.push_back(j["key"]);
        env_nums.push_back(j["num"]);
        descriptions.push_back(j["description"]);
        std::string env_path = "null";
        if (!j["path"].empty()) {
            if (m_option=="dcs") env_path = j["path"];
            else if (m_option=="db") env_path = std::string(m_cache_json["logPath"])+"/"+std::string(j["path"]);
        }
        env_paths.push_back(env_path);
        if (!j["value"].empty()) env_vals.push_back(j["value"]);
        else env_vals.push_back(-1); 
        if (!j["margin"].empty()) env_margins.push_back(j["margin"]);
        else env_margins.push_back(-1);
    }

    // get start time from scan data
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(result->view()["startTime"].get_date().value);
    std::time_t starttime = s.count();
    if (DB_DEBUG) {
        char buf[80];
        struct tm *lt = std::localtime(&starttime);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
        std::cout << "\tDBHandler: Register Environment Start Time: " << buf << std::endl;
    }

    std::chrono::seconds f = std::chrono::duration_cast<std::chrono::seconds>(result->view()["finishTime"].get_date().value);
    std::time_t finishtime = f.count();
    if (DB_DEBUG) {
        char buf[80];
        struct tm *lt = std::localtime(&finishtime);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
        std::cout << "\tDBHandler: Register Environment Finish Time: " << buf << std::endl;
    }

    // insert environment doc
    document builder{};
    auto docs = builder << 
        "dbVersion" << -1 <<
        "sys"       << open_document << close_document;
    for (unsigned i=0;i<env_keys.size();i++) {
        docs = docs << env_keys[i] << open_array << close_array;
    }
    bsoncxx::document::value doc_value2 = docs << finalize;
    bsoncxx::oid env_oid = collection.insert_one(doc_value2.view())->inserted_id().get_oid().value;

    for (int i=0; i<(int)env_keys.size(); i++) {
        if (DB_DEBUG) std::cout << "\tDBHandler: Register Environment: " << env_keys[i] << std::endl;
        if (env_paths[i]!="null") {
            std::ifstream env_ifs(env_paths[i]);
            std::size_t suffix = env_paths[i].find_last_of('.');
            std::string extension = env_paths[i].substr(suffix + 1);
            std::string del;
            if (extension=="dat") del = " ";
            else if (extension=="csv") del = ",";
            char separator = del[0];

            char tmp_key[1000], tmp_num[1000], tmp[1000];
            int data_num = 0;
            std::vector<float> key_values;
            std::vector<int> dates;
            // key and num
            env_ifs.getline(tmp_key, 1000);
            env_ifs.getline(tmp_num, 1000);
            int columns = 0;
            int key=-1;
            for (const auto s_tmp : split(tmp_key, separator)) {
                if (env_keys[i]==s_tmp) {
                    int cnt=0;
                    for (const auto s_tmp_num : split(tmp_num, separator)) {
                        if (cnt==columns&&env_nums[i]==stoi(s_tmp_num)) key = columns;
                        cnt++;
                    }
                }
                columns++;
            }
            // mode
            env_ifs.getline(tmp, 1000);
            int cnt = 0;
            for (const auto s_tmp : split(tmp, separator)) {
                if (cnt==key) env_modes.push_back(s_tmp);
                cnt++;
            }
            // setting
            env_ifs.getline(tmp, 1000);
            cnt = 0;
            for (const auto s_tmp : split(tmp, separator)) {
                if (cnt==key) env_settings.push_back(stof(s_tmp));
                cnt++;
            }
            // value
            while (env_ifs.getline(tmp, 1000)) {
                std::string datetime = "";
                std::string value = "";
                cnt = 0;
                for (const auto s_tmp : split(tmp, separator)) {
                    if (cnt==1) datetime = s_tmp;    
                    if (cnt==key) value = s_tmp;
                    cnt++;
                }
                if (value==""||datetime == "") break;
                std::time_t timestamp = stoi(datetime);
                if (env_margins[i]!=-1) {
                    if (difftime(starttime,timestamp)<env_margins[i]&&difftime(timestamp,finishtime)<env_margins[i]) { // store data from 1 minute before the starting time of the scan
                        key_values.push_back(stof(value));
                        dates.push_back(stoi(datetime));
                        data_num++;
                    }
                } else {
                    key_values.push_back(stof(value));
                    dates.push_back(stoi(datetime));
                    data_num++;
                }
            }
            auto array_builder = bsoncxx::builder::basic::array{};
            for (int j=0;j<data_num;j++) {
                std::time_t timestamp = dates[j];
                array_builder.append(
                    document{} << 
                        "date"        << bsoncxx::types::b_date{std::chrono::system_clock::from_time_t(timestamp)} <<
                        "value"       << key_values[j] <<
                    finalize
                ); 
            }
            collection.update_one(
                document{} << "_id" << env_oid << finalize,
                document{} << "$push" << open_document <<
                    env_keys[i] << open_document <<
                        "data"        << array_builder <<
                        "description" << descriptions[i] <<
                        "mode" << env_modes[i] <<
                        "setting" << env_settings[i] <<
                        "num" << env_nums[i] <<
                    close_document <<
                close_document << finalize
            );
        } else {
            auto array_builder = bsoncxx::builder::basic::array{};
            array_builder.append(
                document{} << 
                    "date"        << bsoncxx::types::b_date{std::chrono::system_clock::from_time_t(starttime)} <<
                    "value"       << env_vals[i] <<
                finalize
            ); 
            collection.update_one(
                document{} << "_id" << env_oid << finalize,
                document{} << "$push" << open_document <<
                    env_keys[i] << open_document <<
                        "data"        << array_builder <<
                        "description" << descriptions[i] <<
                    close_document <<
                close_document << finalize
            );
        }
    }
    this->addSys(env_oid.to_string(), "environment");
    this->addVersion("environment", "_id", env_oid.to_string(), "oid");
    this->addValue(m_tr_oid_str, "testRun", "environment", env_oid.to_string());

    return;
}

std::string DBHandler::registerJsonCode(std::string i_file_path, std::string i_filename, std::string i_title, std::string i_type) {
    if (DB_DEBUG) std::cout << "\tDBHandler: upload json file" << std::endl;

    std::string type_doc;
    std::string data_id;
    if (i_type == "m") {
        data_id = writeJsonCode_Msgpack(i_file_path, i_filename, i_title);
        type_doc = "msgpack";
    } else if (i_type == "gj") {
        data_id = writeJsonCode_Gridfs(i_file_path, i_filename, i_title);
        type_doc = "fs.files";
    } else {
        std::string message = "Unknown type '" + i_type + "' to upload json file.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return "ERROR";
    } 
    mongocxx::collection collection = db["config"];
    bsoncxx::document::value doc_value = document{} << 
      "sys"       << open_document << close_document <<
      "filename"  << i_filename <<
      "chipType"  << m_chip_type << 
      "title"     << i_title <<
      "format"    << type_doc <<
      "data_id"   << data_id <<
      "dbVersion" << -1 <<
    finalize; 
    std::string oid_str = collection.insert_one(doc_value.view())->inserted_id().get_oid().value.to_string();
    this->addSys(oid_str, "config");
    this->addVersion("config", "_id", oid_str, "oid");
    return oid_str;
}

void DBHandler::registerComponentTestRun(std::string i_conn_path, std::string i_tr_oid_str, std::string i_test_type, int i_run_number) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register Com-Test Run" << std::endl;

    json conn_json = this->toJson(i_conn_path);
    std::vector<std::string> cmp_oid_strs;
    std::string mo_serial_number = conn_json["module"]["serialNumber"];
    if (conn_json["dummy"]) {
        cmp_oid_strs.push_back(mo_serial_number);
        for (auto chip_json : conn_json["chips"]) {
            std::string ch_serial_number = chip_json["serialNumber"];
            cmp_oid_strs.push_back(ch_serial_number);
        }
    } else {
        std::string mo_oid_str = this->getComponent(mo_serial_number);
        if (mo_oid_str!="") {
            cmp_oid_strs.push_back(mo_oid_str);
            auto doc_value = document{} <<
                "parent" << mo_oid_str <<
                "status" << "active" <<
            finalize;
            mongocxx::cursor cursor = db["childParentRelation"].find(doc_value.view());
            for (auto doc : cursor) {
                std::string chip_oid_str = doc["child"].get_utf8().value.to_string();
                cmp_oid_strs.push_back(chip_oid_str);
            }
        } else {
            std::string message = "This module "+mo_serial_number+" is not registered: "+i_conn_path;
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message);
        }
    }
    for (auto cmp_oid_str: cmp_oid_strs) {
        std::string serial_number;
        if (conn_json["dummy"]) {
            serial_number = cmp_oid_str;
        } else {
            serial_number = this->getValue("component", "_id", cmp_oid_str, "oid", "serialNumber");
        }
        int chip_tx = -1;
        int chip_rx = -1;
        int geom_id = -1;
        int chip_id = -1;
        for (unsigned i=0; i<conn_json["chips"].size(); i++) {
            if (conn_json["chips"][i]["serialNumber"]==serial_number) {
                chip_tx = conn_json["chips"][i]["tx"];
                chip_rx = conn_json["chips"][i]["rx"];
                geom_id = conn_json["chips"][i]["geomId"];
                chip_id = conn_json["chips"][i]["chipId"];
                break;
            }
        }
        auto doc_value = document{} <<  
            "sys"         << open_document << close_document <<
            "component"   << cmp_oid_str << // id of component
            "state"       << "..." << // code of test run state
            "testType"    << i_test_type << // id of test type
            "testRun"     << i_tr_oid_str << // id of test run, test is planned if it is null
            "qaTest"      << false << // flag if it is the QA test
            "runNumber"   << i_run_number << // run number
            "passed"      << true << // flag if the test passed
            "problems"    << true << // flag if any problem occured
            "attachments" << open_array << close_array <<
            "tx"          << chip_tx <<
            "rx"          << chip_rx <<
            "chipId"      << chip_id <<
            "beforeCfg"   << "..." <<
            "afterCfg"    << "..." <<
            "geomId"      << geom_id <<
            "dbVersion"   << -1 <<
        finalize;
        mongocxx::collection collection = db["componentTestRun"];
        bsoncxx::oid oid = collection.insert_one(doc_value.view())->inserted_id().get_oid().value;
        this->addSys(oid.to_string(), "componentTestRun");
        this->addVersion("componentTestRun", "_id", oid.to_string(), "oid");
    }
}

std::string DBHandler::registerTestRun(std::string i_test_type, int i_run_number, int i_target_charge, int i_target_tot, int i_time, std::string i_serial_number, std::string i_type, std::string i_tr_oid_str) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Register Test Run" << std::endl;

    mongocxx::collection collection = db["testRun"];
    std::string oid_str;

    if (i_type=="start") {
        std::time_t timestamp = i_time;
        auto startTime = std::chrono::system_clock::from_time_t(timestamp);
        auto doc_value = document{} <<
            "startTime"    << bsoncxx::types::b_date{startTime} <<
            "serialNumber" << i_serial_number <<
        finalize;
        auto maybe_result = collection.find_one(doc_value.view());
        if (maybe_result) return maybe_result->view()["_id"].get_oid().value.to_string();

        doc_value = document{} <<  
            "sys"          << open_document << close_document <<
            "testType"     << i_test_type << // id of test type //TODO make it id
            "runNumber"    << i_run_number << // number of test run
            "startTime"    << bsoncxx::types::b_date{startTime} << // date when the test run was taken
            "passed"       << false << // flag if test passed
            "problems"     << false << // flag if any problem occured
            "summary"      << false << // summary plot
            "dummy"        << false <<
            "state"        << "ready" << // state of component ["ready", "requestedToTrash", "trashed"]
            "targetCharge" << i_target_charge <<
            "targetTot"    << i_target_tot <<
            "command"      << "..." <<
            "comments"     << open_array << close_array <<
            "defects"      << open_array << close_array <<
            "finishTime"   << bsoncxx::types::b_date{startTime} <<
            "plots"        << open_array << close_array <<
            "serialNumber" << i_serial_number << // module serial number
            "chipType"     << m_chip_type <<
            "stage"        << "..." <<
            "ctrlCfg"      << "..." << 
            "scanCfg"      << "..." <<
            "environment"  << "..." <<
            "address"      << "..." <<
            "user_id"      << "..." << 
            "dbVersion"    << -1 << 
        finalize;

        auto oid = collection.insert_one(doc_value.view())->inserted_id().get_oid().value;
        oid_str = oid.to_string();
        this->addSys(oid_str, "testRun");
        this->addVersion("testRun", "_id", oid_str, "oid");
        this->addUser("testRun", oid_str);
    }
    if (i_type=="finish") {
        std::time_t timestamp = i_time;
        auto finishTime = std::chrono::system_clock::from_time_t(timestamp);
        auto array_builder = bsoncxx::builder::basic::array{};
        for (auto s_tmp: m_histo_names) array_builder.append(s_tmp); 
        bsoncxx::document::value doc_value = document{} <<  
            "passed"       << true << // flag if test passed
            "problems"     << true << // flag if any problem occured
            "finishTime"   << bsoncxx::types::b_date{finishTime} <<
            "plots"        << array_builder <<
        finalize;
        collection.update_one(
            document{} << "_id" << bsoncxx::oid(i_tr_oid_str) << finalize,
            document{} << "$set" << doc_value.view() << finalize
        );
        oid_str = i_tr_oid_str;
    }
    return oid_str;
}

// Add function
void DBHandler::addComment(std::string i_collection_name, std::string i_oid_str, std::string i_comment) { // To be deleted or seperated
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add comment" << std::endl;
    bsoncxx::oid i_oid(i_oid_str);
    db[i_collection_name].update_one(
        document{} << "_id" << i_oid << finalize,
        document{} << "$push" << open_document <<
            "comments" << open_document <<
                    "code" << "01234567890abcdef01234567890abcdef" << // generated unique code
                    "dateTime" << bsoncxx::types::b_date{std::chrono::system_clock::now()} << // comment creation timestamp
                    "comment" << i_comment << // text of comment
            close_document <<
        close_document << finalize
    );
}

void DBHandler::addValue(std::string i_oid_str, std::string i_collection_name, std::string i_key, std::string i_value, std::string i_type) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add document: " << i_key << " to " << i_collection_name << std::endl;
    bsoncxx::oid i_oid(i_oid_str);
    if (i_type=="string") {
        db[i_collection_name].update_one(
            document{} << "_id" << i_oid << finalize,
            document{} << "$set" << open_document <<
                i_key << i_value << 
            close_document << finalize
        );
    } else if (i_type=="bool") {
        bool value;
        if (i_value=="true") value = true;
        else value = false;
        db[i_collection_name].update_one(
            document{} << "_id" << i_oid << finalize,
            document{} << "$set" << open_document <<
                i_key << value << 
            close_document << finalize
        );
    } else if (i_type=="int") {
        int value = stoi(i_value);
        db[i_collection_name].update_one(
            document{} << "_id" << i_oid << finalize,
            document{} << "$set" << open_document <<
                i_key << value << 
            close_document << finalize
        );
    }
    return;
}

void DBHandler::addAttachment(std::string i_oid_str, std::string i_collection_name, std::string i_file_oid_str, std::string i_title, std::string i_description, std::string i_content_type, std::string i_filename) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add attachment: " << i_filename << std::endl;
    bsoncxx::oid i_oid(i_oid_str);
    db[i_collection_name].update_one(
        document{} << "_id" << i_oid << finalize,
        document{} << "$push" << open_document <<
            "attachments" << open_document <<
                "code" << i_file_oid_str << // generated unique code
                "dateTime" << bsoncxx::types::b_date{std::chrono::system_clock::now()} << // attachment creation timestamp
                "title" << i_title << // attachment title
                "description" << i_description << // brief description of attachment
                "contentType" << i_content_type << // content type of attachment
                "filename" << i_filename << // file name of attachment
            close_document <<
        close_document << finalize
    );
}

void DBHandler::addDefect(std::string i_oid_str, std::string i_collection_name, std::string i_defect_name, std::string i_description) { // To de deleted
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add defect" << std::endl;
    bsoncxx::oid i_oid(i_oid_str);
    db[i_collection_name].update_one(
        document{} << "_id" << i_oid << finalize,
        document{} << "$push" << open_document <<
            "defects" << open_document <<
                "code" << "01234567890abcdef01234567890abcdef" << // generated unique code
                "dateTime" << bsoncxx::types::b_date{std::chrono::system_clock::now()} << // attachment creation timestamp
                "name" << i_defect_name << // defect name
                "description" << i_description << // defect description
                "properties" << "..." << // properties object, optional
            close_document <<
        close_document << finalize
    );
}

void DBHandler::addUser(std::string i_collection_name, std::string i_oid_str) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add user and institution" << std::endl;
    bsoncxx::oid i_oid(i_oid_str);

    // Update document
    db[i_collection_name].update_one(
        document{} << "_id" << i_oid << finalize,
        document{} << "$set" << open_document <<
            "address" << m_site_oid_str <<
            "user_id" << m_user_oid_str <<
        close_document << finalize
    );
}

void DBHandler::addSys(std::string i_oid_str, std::string i_collection_name) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add sys" << std::endl;
    bsoncxx::oid i_oid(i_oid_str);
    db[i_collection_name].update_one(
        document{} << "_id" << i_oid << finalize,
        document{} << "$set" << open_document <<
            "sys" << open_document <<
                "rev" << 0 << // revision number
                "cts" << bsoncxx::types::b_date{std::chrono::system_clock::now()} << // creation timestamp
                "mts" << bsoncxx::types::b_date{std::chrono::system_clock::now()} << // modification timestamp
            close_document <<
        close_document << finalize
    );
}

void DBHandler::addVersion(std::string i_collection_name, std::string i_member_key, std::string i_member_value, std::string i_member_bson_type) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Add DB Version " << m_db_version << std::endl;
    if (i_member_bson_type == "oid") {
        db[i_collection_name].update_many(
            document{} << i_member_key << bsoncxx::oid(i_member_value) << finalize,
            document{} << "$set" << open_document <<
                "dbVersion" << m_db_version <<
            close_document << finalize
        );
    } else {
        db[i_collection_name].update_many(
            document{} << i_member_key << i_member_value << finalize,
            document{} << "$set" << open_document <<
                "dbVersion" << m_db_version <<
            close_document << finalize
        );
    }
}

void DBHandler::writeScan(std::string i_cache_dir) {
    if (DB_DEBUG) std::cout << "DBHandler: Write scan data from cache: " << i_cache_dir << std::endl;

    // check cache directory
    if (m_cache_json["status"]=="done") {
        std::cout << "Already uploaded, then move cache files to temporary..." << std::endl;
        int now = std::time(NULL);
        std::string cmd = "mv "+i_cache_dir+" "+m_cache_dir+"/tmp/localdb/done/"+std::to_string(now);
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem moving directory "+i_cache_dir+" into "+m_cache_dir+"/tmp/localdb/done/"+std::to_string(now);
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        return;
    } else if (m_cache_json["status"]=="failure") {
        int now = std::time(NULL);
        std::string cmd = "mv "+i_cache_dir+" "+m_cache_dir+"/tmp/localdb/failed/"+std::to_string(now);
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem moving directory "+i_cache_dir+" into "+m_cache_dir+"/tmp/localdb/failed/"+std::to_string(now);
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        return;
    }
    if (m_cache_json["status"]!="waiting") return;
    // write status
    this->writeJson("status", "writing", m_cache_path, m_cache_json);

    // set user
    this->setUser(i_cache_dir+"/user.json");
    this->setSite(i_cache_dir+"/address.json");

    // set connectivity config
    std::vector<std::string> conn_paths;
    int timestamp = m_cache_json["startTime"];
    for (int i=0; i<(int) m_cache_json["configs"]["connCfg"].size(); i++) {
        std::string conn_path = i_cache_dir+"/"+std::string(m_cache_json["configs"]["connCfg"][i]["path"]);
        json conn_json = this->toJson(conn_path);
        std::string mo_serial_number = conn_json["module"]["serialNumber"];
        this->getTestRunData("", mo_serial_number, timestamp);
        if (m_tr_oid_str!="") {
            std::string message = "Already registered test run data in DB, then skip to save...";
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message, "warning");
            continue;
        }
        conn_paths.push_back(conn_path);
    }
    this->setConnCfg(conn_paths);

    // write test run
    std::string scan_type = m_cache_json["testType"];
    int run_number        = m_cache_json["runNumber"];
    int target_charge     = m_cache_json["targetCharge"];
    int target_tot        = m_cache_json["targetTot"];
    int start_timestamp   = m_cache_json["startTime"];
    std::string command   = m_cache_json["command"];
    this->setTestRunStart(scan_type, conn_paths, run_number, target_charge, target_tot, start_timestamp, command);

    // write config
    // controller config
    std::string ctrl_cfg_path = i_cache_dir+"/"+std::string(m_cache_json["configs"]["ctrlCfg"][0]["path"]);
    this->setConfig(-1, -1, ctrl_cfg_path, "controller", "ctrlCfg", "testRun", "null"); //controller config

    // scan config
    std::string scan_cfg_path;
    if (!m_cache_json["configs"]["scanCfg"].empty()) scan_cfg_path = i_cache_dir+"/"+std::string(m_cache_json["configs"]["scanCfg"][0]["path"]);
    else scan_cfg_path = "";
    this->setConfig(-1, -1, scan_cfg_path, scan_type, "scanCfg", "testRun", "null"); 

    // chip config
    for (auto chip_json: m_cache_json["configs"]["chipCfg"]) {
        std::string chip_id   = chip_json["_id"];
        std::string chip_path = i_cache_dir+"/"+std::string(chip_json["path"]);
        std::cout << chip_path << std::endl;
        std::string filename  = chip_json["filename"];
        std::string title     = chip_json["title"];
        this->setConfig(-1, -1, chip_path, filename, title, "componentTestRun", chip_id);
    }

    // attachments
    for (auto attachment: m_cache_json["attachments"]) {
        std::string chip_id   = attachment["_id"];
        std::string file_path = i_cache_dir+"/"+std::string(attachment["path"]);
        std::string histoname = attachment["histoname"];
        this->setAttachment(-1, -1, file_path, histoname, chip_id);
    }

    // finish
    int finish_timestamp = m_cache_json["finishTime"];
    this->setTestRunFinish(scan_type, conn_paths, run_number, target_charge, target_tot, finish_timestamp, command);

    //for (auto serial_number : m_serial_numbers) {
    //    int start_time = m_cache_json["startTime"];
    //    this->getTestRunData("", serial_number, start_time);
    //    if (m_tr_oid_str=="") {
    //        m_log_json["errormessage"] = "Failed to upload, try again";
    //        m_cache_json["errormessage"] = "Failed to upload, try again";
    //        this->writeJson("status", "failure", m_cache_path, m_cache_json);
    //        this->writeJson("status", "failure", m_log_path, m_log_json);
    //    }
    //}
    //if (m_cache_json["status"]!="failure") this->writeJson("status", "done", m_cache_path, m_cache_json);
    //if (m_log_json["status"]!="failure") this->writeJson("status", "done", m_log_path, m_log_json);
    return;
}

void DBHandler::writeDCS(std::string i_cache_dir) {
    if (DB_DEBUG) std::cout << "DBHandler: Write DCS data from cache: " << i_cache_dir << std::endl;

    // check cache directory
    if (m_cache_json["status"]=="done") {
        std::cout << "Already uploaded, then move cache files to temporary..." << std::endl;
        int now = std::time(NULL);
        std::string cmd = "mv "+i_cache_dir+" "+m_cache_dir+"/tmp/localdb/done/"+std::to_string(now);
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem moving directory "+i_cache_dir+" into "+m_cache_dir+"/tmp/localdb/done/"+std::to_string(now);
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        return;
    } else if (m_cache_json["status"]=="failure") {
        int now = std::time(NULL);
        std::string cmd = "mv "+i_cache_dir+" "+m_cache_dir+"/tmp/localdb/failed/"+std::to_string(now);
        if (system(cmd.c_str()) < 0) {
            std::string message = "Problem moving directory "+i_cache_dir+" into "+m_cache_dir+"/tmp/localdb/failed/"+std::to_string(now);
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        return;
    }
    if (m_cache_json["status"]!="waiting") return;

    std::string tr_file_path = i_cache_dir+"/tr.json";
    std::string dcs_file_path = i_cache_dir+"/dcs.json";
    // check test run data registered
    json tr_json = this->toJson(tr_file_path);
    if (tr_json["id"].empty()) {
        this->checkEmpty(tr_json["startTime"].empty()||tr_json["serialNumber"].empty(), "testRun.startTime&&testRun.serialNumber", tr_file_path);
    }
    std::string tr_oid_str = "";
    std::string serial_number = "";
    int timestamp = -1;
    if (!tr_json["id"].empty()) tr_oid_str = tr_json["id"];
    if (!tr_json["serialNumber"].empty()) serial_number = tr_json["serialNumber"];
    if (!tr_json["startTime"].empty()) timestamp = tr_json["startTime"];
    this->getTestRunData(tr_oid_str, serial_number, timestamp);
    if (m_tr_oid_str=="") {
        std::string message = "Not found relational test run data in DB";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message, "warning"); return;
    }

    // write status
    this->writeJson("status", "writing", m_cache_path, m_cache_json);

    this->setDCSCfg(dcs_file_path, tr_file_path);
    this->registerDCS(dcs_file_path, tr_file_path);

//    auto doc_value = make_document(kvp("_id", bsoncxx::oid(m_tr_oid_str)));
//    auto result = db["testRun"].find_one(doc_value.view());
//    if (result->view()["environment"].get_utf8().value.to_string()=="...") {
//        m_log_json["errormessage"] = "Failed to upload, try again";
//        m_cache_json["errormessage"] = "Failed to upload, try again";
//        this->writeJson("status", "failure", m_log_path, m_log_json);
//        this->writeJson("status", "failure", m_cache_path, m_cache_json);
//    }
//    if (m_cache_json["status"]!="failure") this->writeJson("status", "done", m_cache_path, m_cache_json);
//    if (m_log_json["status"]!="failure") this->writeJson("status", "done", m_log_path, m_log_json);

    return;
}

std::string DBHandler::writeGridFsFile(std::string i_file_path, std::string i_filename) {
    if (DB_DEBUG) std::cout << "DBHandler: Write attachment: " << i_file_path << std::endl;
    mongocxx::gridfs::bucket gb = db.gridfs_bucket();
    mongocxx::collection collection = db["fs.files"];
      
    std::ifstream file_ifs(i_file_path);
    std::istream &file_is = file_ifs;
    std::string oid_str = gb.upload_from_stream(i_filename, &file_is).id().get_oid().value.to_string();
    file_ifs.close();

    this->addVersion("fs.files", "_id", oid_str, "oid");
    this->addVersion("fs.chunks", "files_id", oid_str, "oid");
    return oid_str;
}

std::string DBHandler::writeJsonCode_Msgpack(std::string i_file_path, std::string i_filename, std::string i_title) {
    if (DB_DEBUG) std::cout << "DBHandler: Write json file: " << i_file_path << std::endl;

    mongocxx::collection collection = db["msgpack"];
    std::string hash = this->getHash(i_file_path);
    mongocxx::options::find opts;
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = collection.find_one(
        document{} << "hash" << hash << finalize,
        opts.return_key(true)
    );
    if (maybe_result) return maybe_result->view()["_id"].get_oid().value.to_string();

    std::ifstream file_ifs(i_file_path);
    json file_json = json::parse(file_ifs);
    json gCfg = file_json["RD53A"]["GlobalConfig"];
    std::vector<uint8_t> gl_msgpack = json::to_msgpack(gCfg);
    auto array_builder_gl = bsoncxx::builder::basic::array{};
    for (auto tmp : gl_msgpack) {
        array_builder_gl.append( tmp );
    }

    json pCfg = file_json["RD53A"]["PixelConfig"];
    std::vector<uint8_t> pi_msgpack = json::to_msgpack(file_json["RD53A"]["PixelConfig"]);
    auto array_builder_pi = bsoncxx::builder::basic::array{};
    for (auto tmp : pi_msgpack) {
        array_builder_pi.append( tmp );
    }

    json par = file_json["RD53A"]["Parameter"];
    bsoncxx::document::value par_doc = bsoncxx::from_json(par.dump()); 

    bsoncxx::document::value doc_value = document{} << 
        "data" << open_document <<
            "GlobalConfig" << array_builder_gl << 
            "Parameter"    << par_doc.view() << 
            "PixelConfig"  << array_builder_pi << 
        close_document <<
        "hash" << hash <<
        "dbVersion" << -1 << 
    finalize; 
    std::string oid_str = collection.insert_one(doc_value.view())->inserted_id().get_oid().value.to_string();
    this->addVersion("json", "_id", oid_str, "oid");
    return oid_str;
}

std::string DBHandler::writeJsonCode_Gridfs(std::string i_file_path, std::string i_filename, std::string i_title) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Write json file: " << i_file_path << std::endl;

    std::string hash = this->getHash(i_file_path);
    mongocxx::options::find opts;
    bsoncxx::stdx::optional<bsoncxx::document::value> maybe_result = db["fs.files"].find_one(
        document{} << "hash" << hash << finalize,
        opts.return_key(true)
    );
    if (maybe_result) return maybe_result->view()["_id"].get_oid().value.to_string();

    std::string oid_str = this->writeGridFsFile(i_file_path, i_filename); 
    db["fs.files"].update_one(
        document{} << "_id" << bsoncxx::oid(oid_str) << finalize,
        document{} << "$set" << open_document <<
            "hash" << hash << 
        close_document << finalize
    );
    this->addVersion("fs.files", "_id", oid_str, "oid");
    this->addVersion("fs.chunks", "files_id", oid_str, "oid");
    return oid_str;
}

std::string DBHandler::getHash(std::string i_file_path) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Get hash code from: " << i_file_path << std::endl;
    SHA256_CTX context;
    if (!SHA256_Init(&context)) return "ERROR"; 
    static const int K_READ_BUF_SIZE{ 1024*16 };
    char buf[K_READ_BUF_SIZE];
    std::ifstream file(i_file_path, std::ifstream::binary);
    while (file.good()) {
        file.read(buf, sizeof(buf));
        if(!SHA256_Update(&context, buf, file.gcount())) return "ERROR";
    }
    unsigned char result[SHA256_DIGEST_LENGTH];
    if (!SHA256_Final(result, &context)) return "ERROR";
    std::stringstream shastr;
    shastr << std::hex << std::setfill('0');
    for (const auto &byte: result) {
        shastr << std::setw(2) << (int)byte;
    }
    return shastr.str();
}

void DBHandler::getTestRunLog(json i_info_json) {
    if (DB_DEBUG) std::cout << "DBHandler: Get TestRun Log" << std::endl;

    this->checkEmpty(i_info_json["serialNumber"].empty(), "serialNumber", "");
    std::string serial_number = i_info_json["serialNumber"];
    document builder{};
    auto docs = builder << "serialNumber" << serial_number;
    for (int i=0;i<(int)i_info_json["options"].size();i++) {
        this->checkEmpty(i_info_json["options"][i]["status"].empty(), "options."+std::to_string(i)+".status", "Set enabled/disabled.");
        if (i_info_json["options"][i]["status"]!="enabled") continue;
        this->checkEmpty(i_info_json["options"][i]["key"].empty(), "options."+std::to_string(i)+".key", "Set option key.");
        this->checkEmpty(i_info_json["options"][i]["value"].empty(), "options."+std::to_string(i)+".value", "Set option value.");
        this->checkEmpty(i_info_json["options"][i]["type"].empty(), "options."+std::to_string(i)+".type", "Set option value type num/str.");
        if (i_info_json["options"][i]["type"]=="str") {
            std::string key = i_info_json["options"][i]["key"];
            std::string value = i_info_json["options"][i]["value"];
            docs = docs << key << value;
        } else if (i_info_json["options"][i]["type"]=="num") {
            std::string key = i_info_json["options"][i]["key"];
            float value = i_info_json["options"][i]["value"];
            docs = docs << key << value;
        }
    }
    auto doc_value = docs << finalize;
    mongocxx::options::find opts;
    opts.sort(make_document(kvp("$natural", -1)));
    if (!i_info_json["limit"].empty()) {
        int limit_num = i_info_json["limit"];
        opts.limit(limit_num);
    }
    mongocxx::cursor cursor = db["testRun"].find(doc_value.view(), opts);
    for (auto doc : cursor) {
        std::string oid_str = doc["_id"].get_oid().value.to_string();
        std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(doc["startTime"].get_date().value);
        std::time_t starttime = s.count();
        char ts[80];
        struct tm *lt = std::localtime(&starttime);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);
        std::string startTime = ts;

        std::string user_name = this->getValue("user", "_id", doc["user_id"].get_utf8().value.to_string(), "oid", "userName");
        std::string site_name = this->getValue("institution", "_id", doc["address"].get_utf8().value.to_string(), "oid", "institution");

        std::cout << "\033[1;33mtestRun data ID: " << oid_str << "\033[0m" << std::endl;
        std::cout << "User: " << user_name << " at " << site_name << std::endl;
        std::cout << "Date: " << startTime << std::endl;
        std::cout << "\tRun Number: " << std::to_string(doc["runNumber"].get_int32().value) << std::endl;
        std::cout << "\tTest Type:  " << doc["testType"].get_utf8().value.to_string() << std::endl;
        std::cout << "\t\033[1;33mctrlCfg:    " << doc["ctrlCfg"].get_utf8().value.to_string() << "\033[0m" << std::endl;
        std::cout << "\t\033[1;33mscanCfg:    " << doc["scanCfg"].get_utf8().value.to_string() << "\033[0m" << std::endl;

        doc_value = document{} <<
            "testRun" << oid_str <<
        finalize;
        mongocxx::cursor ctr_cursor = db["componentTestRun"].find(doc_value.view());
        for (auto ctr_doc : ctr_cursor) {
            std::string ch_oid_str = ctr_doc["component"].get_utf8().value.to_string();
            std::string ch_serial_number = this->getValue("component", "_id", ch_oid_str, "oid", "serialNumber");
            if (serial_number==ch_serial_number) continue;
            std::cout << "\tChip Serial Number: " << ch_serial_number << std::endl;
            std::cout << "\t\t\033[1;33mbeforeCfg: " << ctr_doc["beforeCfg"].get_utf8().value.to_string() << "\033[0m" << std::endl;
            std::cout << "\t\t\033[1;33mafterCfg:  " << ctr_doc["afterCfg"].get_utf8().value.to_string() << "\033[0m" << std::endl;
        }
        std::cout << std::endl;
    }
    return;
}

void DBHandler::getTestRunId(json i_info_json) {
    if (DB_DEBUG) std::cout << "DBHandler: Get TestRun Id" << std::endl;

    this->checkEmpty(i_info_json["serialNumber"].empty(), "serialNumber", "");
    std::string serial_number = i_info_json["serialNumber"];
    document builder{};
    auto docs = builder << "serialNumber" << serial_number;
    for (int i=0;i<(int)i_info_json["options"].size();i++) {
        this->checkEmpty(i_info_json["options"][i]["status"].empty(), "options."+std::to_string(i)+".status", "Set enabled/disabled.");
        if (i_info_json["options"][i]["status"]!="enabled") continue;
        this->checkEmpty(i_info_json["options"][i]["key"].empty(), "options."+std::to_string(i)+".key", "Set option key.");
        this->checkEmpty(i_info_json["options"][i]["value"].empty(), "options."+std::to_string(i)+".value", "Set option value.");
        this->checkEmpty(i_info_json["options"][i]["type"].empty(), "options."+std::to_string(i)+".type", "Set option value type num/str.");
        if (i_info_json["options"][i]["type"]=="str") {
            std::string key = i_info_json["options"][i]["key"];
            std::string value = i_info_json["options"][i]["value"];
            docs = docs << key << value;
        } else if (i_info_json["options"][i]["type"]=="num") {
            std::string key = i_info_json["options"][i]["key"];
            float value = i_info_json["options"][i]["value"];
            docs = docs << key << value;
        }
    }
    auto doc_value = docs << finalize;
    mongocxx::options::find opts;
    opts.sort(make_document(kvp("$natural", -1)));
    opts.limit(1);
    mongocxx::cursor cursor = db["testRun"].find(doc_value.view(), opts);
    for (auto doc : cursor) {
        std::string oid_str = doc["_id"].get_oid().value.to_string();
        std::cout << oid_str << std::endl;
        break;
    }
    return;
}

void DBHandler::getConfigData(json i_info_json) {
    if (DB_DEBUG) std::cout << "DBHandler: Get Config Data" << std::endl;

    std::string config_data_id;
    json data;
    std::vector<std::string> key_list = { "id", "serialNumber", "type", "filePath", "mergePath" };
    for (auto key : key_list) this->checkEmpty(i_info_json[key].empty(), key, "");
    if (i_info_json["id"]!="") {
        config_data_id = i_info_json["id"];
    } else {
        std::string serial_number = i_info_json["serialNumber"];
        std::string cmp_oid_str = this->getComponent(serial_number);
        if (cmp_oid_str=="") {
            std::string message = "Not found the component: "+serial_number;
            std::string function = __PRETTY_FUNCTION__;
            this->alert(function, message); return;
        }
        auto doc_value = make_document(kvp("component", cmp_oid_str));
        mongocxx::options::find opts;
        opts.sort(make_document(kvp("$natural", -1)));
        opts.limit(5);
        mongocxx::cursor cursor = db["componentTestRun"].find(doc_value.view(), opts);
        for (auto doc : cursor) {
            std::string config_type = "afterCfg";
            if (i_info_json["type"]!="") config_type = i_info_json["type"];
            if (config_type=="scanCfg"||config_type=="ctrlCfg") {
                std::string tr_oid_str = doc["testRun"].get_utf8().value.to_string();
                config_data_id = this->getValue("testRun", "_id", tr_oid_str, "oid", config_type);
            } else {
                config_data_id = doc[config_type].get_utf8().value.to_string();
            }
            if (config_data_id=="...") continue;
            else break;
        }
    }
    if (config_data_id=="...") return;
    std::string data_id = this->getValue("config", "_id", config_data_id, "oid", "data_id");
    bsoncxx::oid data_oid(data_id);
    mongocxx::gridfs::bucket gb = db.gridfs_bucket();
    std::ostringstream os;
    bsoncxx::types::value d_id{bsoncxx::types::b_oid{data_oid}};
    gb.download_to_stream(d_id, &os);
    std::string str = os.str();
    try {
        data = json::parse(str);
    } catch (json::parse_error &e) {
        std::string message = "Could not parse "+str+"\n\twhat(): "+e.what();
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    if (i_info_json["mergePath"]!="") {
        std::string merge_file_path = i_info_json["mergePath"];
        json overwrite_json = toJson(merge_file_path);
        data.merge_patch(overwrite_json);
    }
    if (i_info_json["filePath"]!="") {
        std::string file_path = i_info_json["filePath"];
        std::ofstream file_ofs(file_path.c_str());
        file_ofs << std::setw(4) << data;
        file_ofs.close();
    }
    std::cout << std::setw(4) << data;
    
    return;
}

void DBHandler::getScan(json i_info_json) {
//    if (DB_DEBUG) std::cout << "DBHandler: Get Config Data" << std::endl;
//
//    this->checkEmpty(i_info_json["serialNumber"].empty(), "serialNumber", "");
//    std::string serial_number = i_info_json["serialNumber"];
//    std::string cmp_oid_str = this->getComponent(serial_number);
//    if (cmp_oid_str=="") {
//        std::cout << "Not exist the component: " << serial_number << " in Local DB" << std::endl;
//        return;
//    }
//    std::string component_type = this->getValue("component", "_id", cmp_oid_str, "oid", "componentType");
//    if (component_type!="Module") {
//        std::cout << "Not the module component: " << serial_number << std::endl;
//        return;
//    }
//    json conn_json;
//    conn_json["module"]["serialNumber"] = serial_number;
//    conn_json["module"]["componentType"] = component_type;
//    std::string chip_type = this->getValue("component", "_id", cmp_oid_str, "oid", "chipType");
//    conn_json["chipType"] = chip_type;
//    auto doc_value = make_document(kvp("parent", cmp_oid_str));
//    mongocxx::cursor cursor = db["childParentRelation"].find(doc_value.view());
//    for (auto doc: cursor) {
//        json ch_json;
//        std::string ch_oid_str = doc["child"].get_utf8().value.to_string();
//        std::string ch_serial_number = this->getValue("component", "_id", ch_oid_str, "oid", "serialNumber");
//        ch_json["serialNumber"] = ch_serial_number;
//        std::string ch_component_type = this->getValue("component", "_id", ch_oid_str, "oid", "componentType");
//        ch_json["componentType"] = ch_component_type;
//        std::string chip_id = this->getValue("component", "_id", ch_oid_str, "oid", "chipId", "int");
//        ch_json["chipId"] = stoi(chip_id);
//        ch_json["config"] = ch_oid_str;
//        conn_json["chips"].push_back(ch_json);
//    }
//
//    std::string dir_path = serial_number;
//    this->mkdir(dir_path);
//
//    std::string tr_oid_str;
//    if (i_info_json["testRun"]!="") {
//        tr_oid_str = i_info_json["testRun"];
//    } else {
//        std::string serial_number = conn_json["module"]["serialNumber"];
//        std::string cmp_oid_str = this->getComponent(serial_number);
//        doc_value = make_document(kvp("component", cmp_oid_str));
//        mongocxx::options::find opts;
//        opts.sort(make_document(kvp("$natural", -1)));
//        opts.limit(1);
//        cursor = db["componentTestRun"].find(doc_value.view(), opts);
//        for (auto doc : cursor) {
//            tr_oid_str = doc["testRun"].get_utf8().value.to_string();
//            break;
//        }
//    }
//    doc_value = make_document(kvp("testRun", tr_oid_str));
//    cursor = db["componentTestRun"].find(doc_value.view(), opts);
//// TODO
//    for (auto doc : cursor) {
//        for (auto ch_json : conn_json["chips"]) {
//            if (ch_json["config"] == doc["component"].get_utf8().value.to_string()) {
//                json config_json;
//                std::string ch_oid_str = doc["component"].get_utf8().value.to_string();
//                std::string ch_serial_number = this->getValue("component", "_id", ch_oid_str, "oid", "serialNumber");
//                int chip_id = ch_json["chipId"];
//                config_json["serialNumber"] = ch_serial_number;
//                config_json["id"] = "";
//                config_json["type"] = "chipCfg";
//                config_json["filePath"] = serial_number+"/chip"+std::string(chip_id)+".json";
//                config_json["mergePath"] = "";
//                this->getConfigData(config_json);
//            }
//        tr_oid_str = doc["testRun"].get_utf8().value.to_string();
//        break;
//    }
//    
//            std::string config_type = "afterCfg";
//            if (i_info_json["type"]!="") config_type = i_info_json["type"];
//            if (config_type=="scanCfg"||config_type=="ctrlCfg") {
//                std::string tr_oid_str = doc["testRun"].get_utf8().value.to_string();
//                config_data_id = this->getValue("testRun", "_id", tr_oid_str, "oid", config_type);
//            } else {
//                config_data_id = doc[config_type].get_utf8().value.to_string();
//            }
//            if (config_data_id=="...") continue;
//            else break;
//        }
//    }
//
//    if (!i_info_json["filePath"].empty()) {
//        std::string file_path = i_info_json["filePath"];
//        std::ofstream file_ofs(file_path.c_str());
//        file_ofs << std::setw(4) << conn_json;
//        file_ofs.close();
//    }
//    std::cout << std::setw(4) << conn_json;
//    
    return;
}

void DBHandler::getUserId(json i_info_json) {
    if (DB_DEBUG) std::cout << "DBHandler: Get User Id" << std::endl;

    document builder{};
    auto docs = builder << "userType" << "readWrite";
    std::vector<std::string> key_list = { "userName", "institution", "userIdentity" };
    for (auto key : key_list) {
        if (!i_info_json[key].empty()) {
            std::string value = i_info_json[key];
            docs << key << value;
        }
    }
    auto doc_value = docs << finalize;
    mongocxx::options::find opts;
    if (!i_info_json["limit"].empty()) {
        int limit_num = i_info_json["limit"];
        opts.limit(limit_num);
    }
    mongocxx::cursor cursor = db["user"].find(doc_value.view(), opts);
    for (auto doc : cursor) {
        std::string oid_str = doc["_id"].get_oid().value.to_string();

        std::cout << "\033[1;33muser data ID: " << oid_str << "\033[0m" << std::endl;
        std::cout << "\tUser Name:     " << doc["userName"].get_utf8().value.to_string() << std::endl; 
        std::cout << "\tInstitution:   " << doc["institution"].get_utf8().value.to_string() << std::endl; 
        std::cout << "\tUser Identity: " << doc["userIdentity"].get_utf8().value.to_string() << std::endl; 
        std::cout << std::endl;
    }
    return;
}

void DBHandler::getDatData(json i_info_json) {
    if (DB_DEBUG) std::cout << "DBHandler: Get Dat Data" << std::endl;

    // directory to save
    this->checkEmpty(i_info_json["dirPath"].empty(), "dirPath", "");
    std::string dir_path = i_info_json["dirPath"];
    this->mkdir(dir_path);
    std::string cmd = "rm "+dir_path+"/*";
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem removing files in "+dir_path;
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    this->checkEmpty(i_info_json["testRun"].empty()&&i_info_json["serialNumber"].empty(), "testRun||serialNumber", "");
    std::string tr_oid_str;
    if (!i_info_json["serialNumber"].empty()) {
        std::string serial_number = i_info_json["serialNumber"];
        document builder{};
        auto docs = builder << "serialNumber" << serial_number;
        std::vector<std::string> key_list = { "testType", "runNumber", "address", "user_id" };
        for (auto key : key_list) {
            if (!i_info_json[key].empty()) {
                std::string value = i_info_json[key];
                docs = docs << key << value;
            }
        }
        auto doc_value = docs << finalize;
        mongocxx::options::find opts;
        opts.sort(make_document(kvp("$natural", -1)));
        opts.limit(1);
        mongocxx::cursor cursor = db["testRun"].find(doc_value.view(), opts);
        for (auto doc : cursor) {
            tr_oid_str = doc["_id"].get_oid().value.to_string();
            break;
        }
    } else if (!i_info_json["testRun"].empty()) {
        tr_oid_str = i_info_json["testRun"];
    }
    auto doc_value = document{} <<
        "testRun" << tr_oid_str <<
    finalize;
    mongocxx::options::find opts;
    opts.limit(10);
    mongocxx::cursor cursor = db["componentTestRun"].find(doc_value.view(), opts);
    for (auto doc : cursor) {
        std::string cmp_oid_str = doc["component"].get_utf8().value.to_string();
        std::string serial_number = this->getValue("component", "_id", cmp_oid_str, "oid", "serialNumber");
        for (auto attachment : doc["attachments"].get_array().value) {
            std::string file_path = dir_path + "/" + serial_number + "_" + attachment["filename"].get_utf8().value.to_string();
            std::ofstream file_ofs(file_path.c_str());
            bsoncxx::oid data_oid(attachment["code"].get_utf8().value.to_string());
            mongocxx::gridfs::bucket gb = db.gridfs_bucket();
            std::ostringstream os;
            bsoncxx::types::value d_id{bsoncxx::types::b_oid{data_oid}};
            gb.download_to_stream(d_id, &os);
            file_ofs << os.str();
        }
    }
    return;
}

#endif

void DBHandler::getTestRunData(std::string i_tr_oid_str, std::string i_serial_number, int i_time) {
    if (DB_DEBUG) std::cout << "DBHandler: Get TestRun Data" << std::endl;

    m_tr_oid_str = "";

    if (i_tr_oid_str==""&&(i_serial_number==""||i_time==-1)) {
        std::string message = "testRun Id or (serialNumber and startTime) is required to get testRun Data.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }

    if (m_option!="db"&&m_option!="register") return;
#ifdef MONGOCXX_INCLUDE
    if (i_tr_oid_str=="") {
        std::time_t timestamp = i_time;
        auto startTime = std::chrono::system_clock::from_time_t(timestamp);
        auto doc_value = document{} <<
            "startTime"    << bsoncxx::types::b_date{startTime} <<
            "serialNumber" << i_serial_number <<
        finalize;
        auto result = db["testRun"].find_one(doc_value.view());
        if (result) m_tr_oid_str = result->view()["_id"].get_oid().value.to_string();
    } else {
        auto doc_value = make_document(kvp("_id", bsoncxx::oid(i_tr_oid_str)));
        auto result = db["testRun"].find_one(doc_value.view());
        if (result) m_tr_oid_str = result->view()["_id"].get_oid().value.to_string();
    }
#else
    std::cout << "Not found MongoDB C++ Driver!" << std::endl; //TODO check if data is exist in cache file
    m_tr_oid_str = "";
#endif
}

/////////////////
// Cache Function
void DBHandler::cacheUser(std::string i_user_path) {
    // user config
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache user: " << i_user_path << std::endl;

    std::string user_name, institution, user_identity;
    json user_json;
    if (i_user_path=="") {
        user_name = getenv("USER");
        institution = getenv("HOSTNAME");
        user_json["userName"] = user_name;
        user_json["institution"] = institution;
        user_json["userIdentity"] = "default";
    } else {
        user_json = this->toJson(i_user_path);
    }
    std::ofstream cache_user_file(m_log_dir+"/user.json");
    cache_user_file << std::setw(4) << user_json;
    cache_user_file.close();

    return;
}

void DBHandler::cacheSite(std::string i_address_path) {
    // MAC address
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache address: " << i_address_path << std::endl;
    std::string cmd = "cp " + i_address_path + " " + m_log_dir + "/address.json";
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem copying " + i_address_path + " to cache folder.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }

    return;
}
void DBHandler::cacheConnCfg(std::vector<std::string> i_conn_paths) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache connectivity config" << std::endl;
    std::string mo_serial_number;
    for (int i=0; i<(int)i_conn_paths.size(); i++) {
        std::string conn_path = i_conn_paths[i];
        if (DB_DEBUG) std::cout << "\tDBHandler: Cache connectivity config file: " << conn_path << std::endl;

        json conn_json = toJson(conn_path);
        std::string chip_type = conn_json["chipType"];
        if (conn_json["module"]["serialNumber"].empty()) {
            mo_serial_number = "DUMMY_" + std::to_string(i);
            conn_json["module"]["serialNumber"] = mo_serial_number;
            if (conn_json["module"]["componentType"].empty()) conn_json["module"]["componentType"] = "Module"; //TODO for module only currently
            for (unsigned j=0; j<conn_json["chips"].size(); j++) {
                if (conn_json["chips"][j]["serialNumber"].empty()) conn_json["chips"][j]["serialNumber"] = "DUMMY_chip_" + std::to_string(j);
                if (conn_json["chips"][j]["componentType"].empty()) conn_json["chips"][j]["componentType"] = "Front-end Chip";
                if (conn_json["chips"][j]["geomId"].empty()) conn_json["chips"][j]["geomId"] = j+1;
                if (conn_json["chips"][j]["chipId"].empty()) conn_json["chips"][j]["chipId"] = j+1;
            }
            conn_json["dummy"] = true;
        } else {
            mo_serial_number = conn_json["module"]["serialNumber"];
            conn_json["dummy"] = false;
            char tmp[1000];
            std::string del = ",";
            char separator = del[0];
            std::string mod_list_path = m_cache_dir+"/var/lib/localdb/modules.csv";
            std::ifstream list_ifs(mod_list_path);
            while (list_ifs.getline(tmp, 1000)) {
                if (split(tmp, separator).size()==0) continue;
                if (split(tmp, separator)[0] == mo_serial_number) break; 
            }
            for (unsigned j=0; j<conn_json["chips"].size(); j++) {
                int chip_id = conn_json["chips"][j]["chipId"];
                bool is_chip = false;
                std::string ch_serial_number = "";
                for (const auto s_tmp : split(tmp, separator)) {
                    if (is_chip) {
                        ch_serial_number = s_tmp;
                        break;
                    }
                    if (s_tmp==std::to_string(chip_id)) is_chip=true;
                }
                if (conn_json["chips"][j]["serialNumber"].empty()) conn_json["chips"][j]["serialNumber"] = ch_serial_number;
                if (conn_json["chips"][j]["geomId"].empty()) conn_json["chips"][j]["geomId"] = j+1;
            }
        }
        std::ofstream conn_file(m_cache_dir+"/tmp/localdb/conn.json");
        conn_file << std::setw(4) << conn_json;
        conn_file.close();
    
        cacheConfig(mo_serial_number, m_cache_dir+"/tmp/localdb/conn.json", "connectivity", "connCfg", "");
        m_conn_json["connectivity"].push_back(conn_json);
    }

    return;
}

void DBHandler::cacheTestRun(std::string i_test_type, int i_run_number, int i_target_charge, int i_target_tot, int i_start_time, int i_finish_time, std::string i_command) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache Test Run." << std::endl;

    m_log_json["testType"] = i_test_type;
    m_log_json["runNumber"] = i_run_number;
    m_log_json["targetCharge"] = i_target_charge;
    m_log_json["targetTot"] = i_target_tot;
    m_log_json["command"] = i_command;
    if (i_start_time!=-1) m_log_json["startTime"] = i_start_time;
    if (i_finish_time!=-1) m_log_json["finishTime"] = i_finish_time;

    return;
}

void DBHandler::cacheConfig(std::string i_oid_str, std::string i_file_path, std::string i_filename, std::string i_title, std::string i_collection) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache Config Json: " << i_file_path << std::endl;

    counter++;

    std::ifstream file_ifs(i_file_path);
    std::string cmd = "cp "+i_file_path+" "+m_log_dir+"/"+std::to_string(counter)+".json";
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem copying "+i_file_path+" to cache folder.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    json data_json;
    data_json["_id"]        = i_oid_str;
    //data_json["path"]       = m_log_dir+"/"+std::to_string(counter)+".json"; 
    data_json["path"]       = std::to_string(counter)+".json"; 
    data_json["filename"]   = i_filename;
    data_json["title"]      = i_title;
    data_json["collection"] = i_collection;
    m_log_json["configs"][i_title].push_back(data_json);

    return;
}

void DBHandler::cacheAttachment(std::string i_oid_str, std::string i_file_path, std::string i_histo_name) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache Attachment: " << i_file_path << std::endl;

    std::string oid_str;
    std::string cmd = "cp "+i_file_path+" "+m_log_dir+"/"+std::to_string(counter)+".dat";
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem copying "+i_file_path+" to cache folder.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    json data_json;
    data_json["_id"] = i_oid_str;
    //data_json["path"] = m_log_dir+"/"+std::to_string(counter)+".dat"; 
    data_json["path"] = std::to_string(counter)+".dat"; 
    data_json["histoname"] = i_histo_name;
    m_log_json["attachments"].push_back(data_json);
    counter++;

    return;
}

void DBHandler::cacheDCSCfg(std::string i_dcs_path, std::string i_tr_path) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache DCS config: " << i_dcs_path << std::endl;

    json dcs_json = this->toJson(i_dcs_path);
    json env_json = dcs_json["environments"];
    for (int i=0; i<(int)env_json.size(); i++) {
        std::string num_str = std::to_string(i);
        if (env_json[i]["status"]!="enabled") continue;

        std::string j_key = env_json[i]["key"];
        if (!env_json[i]["path"].empty()) {
            std::string log_path = "";
            if (m_option=="dcs") log_path = env_json[i]["path"];
            else if (m_option=="db") log_path = std::string(m_cache_json["logPath"])+"/"+std::string(env_json[i]["path"]);
            std::size_t suffix = log_path.find_last_of('.');
            std::string extension = log_path.substr(suffix + 1);

            std::string cmd = "cp "+log_path+" "+m_log_dir+"/"+std::to_string(i)+"."+extension;
            if (system(cmd.c_str()) < 0) {
                std::string message = "Problem copying "+log_path+" to cache folder.";
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message); return;
            }
            //env_json[i]["path"] = m_log_dir+"/"+std::to_string(i)+"."+extension;
            env_json[i]["path"] = std::to_string(i)+"."+extension;
        }
    }
    dcs_json["environments"] = env_json;
    std::ofstream cache_dcs_file(m_log_dir+"/dcs.json");
    cache_dcs_file << std::setw(4) << dcs_json;
    cache_dcs_file.close();

    std::string cmd = "cp "+i_tr_path+" "+m_log_dir+"/tr.json";
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem copying "+i_tr_path+" to cache folder.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }

    return;
}

void DBHandler::cacheDBCfg() {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache Database Config: " << m_db_cfg_path << std::endl;
    // DB config
    std::string cmd = "cp " + m_db_cfg_path + " " + m_log_dir + "/database.json";
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem copying " + m_db_cfg_path + " to cache folder.";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
}

void DBHandler::writeJson(std::string i_key, std::string i_value, std::string i_file_path, json i_json) {
    if (DB_DEBUG) std::cout << "\tDBHandler: Cache log file: " << i_file_path << std::endl;

    i_json[i_key] = i_value;
    std::ofstream log_file(i_file_path);
    log_file << std::setw(4) << i_json;
    log_file.close();

    return;
}

/////////////////
// Check Function
void DBHandler::checkFile(std::string i_file_path, std::string i_description) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check file: " << i_file_path << std::endl;
    std::ifstream file_ifs(i_file_path);
    if (!file_ifs.is_open()) {
        std::string message = "Not found the file.\n\tfile: " + i_file_path;
        if (i_description!="") message += "    description: " + i_description;
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message);
    }
    file_ifs.close();
    return;
}

void DBHandler::checkEmpty(bool i_empty, std::string i_key, std::string i_file_path, std::string i_description) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check empty: " << i_key << " in " << i_file_path << std::endl;
    if (i_empty) {
        std::string message = "Found an empty field in json file.\n\tfile: " + i_file_path + "    key: " + i_key;
        if (i_description!="") message += "    description: " + i_description;
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message);
    }
    return;
}

void DBHandler::checkNumber(bool i_number, std::string i_key, std::string i_file_path) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check number: " << i_key << " in " << i_file_path << std::endl;
    if (!i_number) {
        std::string message = "This field value must be the number.\n\tfile: " + i_file_path + "    key: '" + i_key + "'";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message);
    }
    return;
}

void DBHandler::checkList(std::vector<std::string> i_list, std::string i_value, std::string i_list_path, std::string i_file_path) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check list: " << i_value << " in " << i_file_path << std::endl;
    if (std::find(i_list.begin(), i_list.end(), i_value)==i_list.end()) {
        std::string message = "Not registered '" + i_value + "' in " + i_list_path + "\n\tCheck the file: " + i_file_path + "\n\tList : ";
        for (unsigned i=0;i<i_list.size();i++) message += i_list[i] + " ";
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message);
    }
    return;
}

json DBHandler::checkDBCfg(std::string i_db_path) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check database config: " << i_db_path << std::endl;

    json db_json = toJson(i_db_path);
    std::vector<std::string> key_list = { "hostIp", "hostPort", "cachePath", "dbName" };
    for (auto key : key_list) this->checkEmpty(db_json[key].empty(), key, "Set database config by ../localdb/setup_db.sh");

    std::string cache_dir = db_json["cachePath"];
    struct stat statbuf;
    if (stat(cache_dir.c_str(), &statbuf)!=0) {
        std::string message = "Not exist cache directory: "+cache_dir;
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message);
    }

    return db_json;
}

json DBHandler::checkConnCfg(std::string i_conn_path) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check connectivity config: " << i_conn_path << std::endl;
    json conn_json = this->toJson(i_conn_path);
    // chip type
    this->checkEmpty(conn_json["chipType"].empty(), "chipType", i_conn_path);
    // module
    if (!conn_json["module"].empty()) {
        this->checkEmpty(conn_json["module"]["serialNumber"].empty(), "module.serialNumber", i_conn_path); 
        bool is_listed = false;
        char tmp[1000];
        std::string del = ",";
        char separator = del[0];
        if (m_option=="register") {
            this->checkEmpty(conn_json["module"]["componentType"].empty(), "module.componentType", i_conn_path); 
            this->checkList(m_comp_list, std::string(conn_json["module"]["componentType"]), m_db_cfg_path, i_conn_path);
        } else if (m_option=="db") {
        } else {
            std::string mo_serial_number = conn_json["module"]["serialNumber"];
            std::string mod_list_path = m_cache_dir+"/var/lib/localdb/modules.csv";
            std::ifstream list_ifs(mod_list_path);
            if (!list_ifs) {
                std::string message = "Not found modules list: "+mod_list_path;
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message);
            }
            while (list_ifs.getline(tmp, 1000)) {
                if (split(tmp, separator).size()==0) continue;
                if (split(tmp, separator)[0] == mo_serial_number) {
                    is_listed = true;
                    break; 
                }
            }
            if (!is_listed) {
                std::string message = "This module "+mo_serial_number+" is not registered: "+i_conn_path+"\nTry ./bin/dbAccessor -M for pulling component data in local from Local DB";
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message);
            }
        }
        // chips
        for (unsigned i=0; i<conn_json["chips"].size(); i++) {
            this->checkEmpty(conn_json["chips"][i]["chipId"].empty(), "chips."+std::to_string(i)+".chipId", i_conn_path);
            if (m_option=="register") {
                this->checkEmpty(conn_json["chips"][i]["serialNumber"].empty(), "chips."+std::to_string(i)+".serialNumber", i_conn_path);
                this->checkEmpty(conn_json["chips"][i]["componentType"].empty(), "chips."+std::to_string(i)+".componentType", i_conn_path);
                this->checkList(m_comp_list, std::string(conn_json["chips"][i]["componentType"]), m_db_cfg_path, i_conn_path);
            } else if (m_option=="db") {
                this->checkEmpty(conn_json["chips"][i]["serialNumber"].empty(), "chips."+std::to_string(i)+".serialNumber", i_conn_path);
                this->checkEmpty(conn_json["chips"][i]["geomId"].empty(), "chips."+std::to_string(i)+".geomId", i_conn_path);
            } else {
                int chip_id = conn_json["chips"][i]["chipId"];
                is_listed = false;
                for (const auto s_tmp : split(tmp, separator)) {
                    if (s_tmp==std::to_string(chip_id)) {
                        is_listed = true;
                        break;
                    }
                }
                if (!is_listed) {
                    std::string message = "This chip (chipId: "+std::to_string(chip_id)+") is not registered: "+i_conn_path;
                    std::string function = __PRETTY_FUNCTION__;
                    this->alert(function, message);
                }
            }
        }
    }
    // stage
    if (!conn_json["stage"].empty()) {
        std::string stage = conn_json["stage"];
        this->checkList(m_stage_list, stage, m_db_cfg_path, i_conn_path);
    }

    return conn_json;
}

void DBHandler::checkDCSCfg(std::string i_dcs_path, std::string i_num, json i_json) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check DCS config: " << i_dcs_path << std::endl;

    this->checkEmpty(i_json["key"].empty(), "environments."+i_num+".key", i_dcs_path, "Set the environmental key from the key list.");
    std::string j_key = i_json["key"];
    this->checkList(m_env_list, j_key, m_db_cfg_path, i_dcs_path);
    this->checkEmpty(i_json["path"].empty()&&i_json["value"].empty(), "environments."+i_num+".path/value", i_dcs_path);
    this->checkEmpty(i_json["description"].empty(), "environments."+i_num+".description", i_dcs_path);
    this->checkEmpty(i_json["num"].empty(), "environments."+i_num+".num", i_dcs_path);
    this->checkNumber(i_json["num"].is_number(), "environments."+i_num+".num", i_dcs_path);
    if (!i_json["margin"].empty()) this->checkNumber(i_json["margin"].is_number(), "environments."+i_num+".margin", i_dcs_path);

    return;
}

void DBHandler::checkDCSLog(std::string i_log_path, std::string i_dcs_path, std::string i_key, int i_num) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check DCS log file: " << i_log_path << std::endl;

    this->checkFile(i_log_path, "Check environmental data file of key '"+i_key+"' in file "+i_dcs_path+".");
    std::ifstream log_ifs(i_log_path);
    std::size_t suffix = i_log_path.find_last_of('.');
    std::string extension = i_log_path.substr(suffix + 1);
    std::string del;
    if (extension=="dat") del = " ";
    else if (extension=="csv") del = ",";
    else {
        std::string message = "Environmental data file must be 'dat' or 'csv' format: "+i_log_path;
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
    char separator = del[0];
    char tmp[1000];

    std::vector<std::string> log_lines = { "key", "num", "mode", "setting", "value" };
    std::vector<std::string> env_keys;
    int cnt=0;
    int key_cnt=0;
    int columns = 0;
    for (unsigned i=0; i<log_lines.size(); i++) {
        log_ifs.getline(tmp, 1000);
        cnt = 0;
        for (const auto s_tmp : split(tmp, separator)) {
            // check the first column
            if (i!=4&&columns==0&&s_tmp!=log_lines[i]) {
                std::string message = "Set "+log_lines[i]+" in the "+std::to_string(i+1)+ "th line: "+i_log_path;
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message); return;
            }

            // check the values for each line
            if (i==0) {
                env_keys.push_back(s_tmp);
                columns++;
            } else if (i==1&&env_keys[cnt]==i_key) {
                try {
                    if (stoi(s_tmp)==i_num) key_cnt=cnt;
                } catch (const std::invalid_argument& e) {
                    std::string message = "Could not convert the key number text to int: "+i_log_path+"\n\tkey: "+i_key+"\n\ttext: "+s_tmp;
                    std::string function = __PRETTY_FUNCTION__;
                    this->alert(function, message); return;
                }
            } else if (i==4&&cnt==1) {
                try {
                    stoi(s_tmp);
                } catch (const std::invalid_argument& e) {
                    std::string message = "Could not convert the unixtime text to int: "+i_log_path+"\n\ttext: "+s_tmp;
                    std::string function = __PRETTY_FUNCTION__;
                    this->alert(function, message); return;
                }
            } else if ((i==3||i==4)&&(cnt==key_cnt)) {
                try {
                    stof(s_tmp);
                } catch (const std::invalid_argument& e) {
                    std::string message = "Could not convert the setting value text to float: "+i_log_path+"\n\tkey: "+i_key+"\n\ttext: "+s_tmp;
                    std::string function = __PRETTY_FUNCTION__;
                    this->alert(function, message); return;
                }
            }
            cnt++;
        }
        if (i==0) {
            this->checkList(env_keys, i_key, i_log_path, i_dcs_path);
        } else {
            if (cnt!=columns) {
                std::string message = "Not match the number of the "+log_lines[i]+" "+std::to_string(cnt)+" to keys "+std::to_string(columns);
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message); return;
            }
            if (i==1&&key_cnt==0) {
                std::string message = "Environmental key '"+i_key+"' (num: "+std::to_string(i_num)+") was not written in environmental data file: "+i_log_path;
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message); return;
            }
        }
    }
    return;
}

json DBHandler::checkUserCfg(std::string i_user_path) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check user config file: " << i_user_path << std::endl;

    json user_json = this->toJson(i_user_path);
    this->checkEmpty(user_json["userName"].empty(), "userName", i_user_path);
    this->checkEmpty(user_json["institution"].empty(), "institution", i_user_path);
    this->checkEmpty(user_json["userIdentity"].empty(), "userIdentity", i_user_path);
    return user_json;
}

json DBHandler::checkAddressCfg(std::string i_address_path) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Check address config file: " << i_address_path << std::endl;

    json address_json = this->toJson(i_address_path);
    this->checkEmpty(address_json["macAddress"].empty(), "macAddress", i_address_path);
    this->checkEmpty(address_json["hostname"].empty(), "hostname", i_address_path);
    this->checkEmpty(address_json["institution"].empty(), "institution", i_address_path);
    return address_json;
}


//////////
// Others
json DBHandler::toJson(std::string i_file_path, std::string i_file_type) {
    if (DB_DEBUG) std::cout << "\t\tDBHandler: Convert to json code from: " << i_file_path << std::endl;

    std::ifstream file_ifs(i_file_path);
    json file_json;
    if (file_ifs) {
        if (i_file_type=="json") {
            try {
                file_json = json::parse(file_ifs);
            } catch (json::parse_error &e) {
                std::string message = "Could not parse " + i_file_path + "\n\twhat(): " + e.what();
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message);
            }
        } else if (i_file_type=="bson") {
            std::uint8_t tmp;
            std::vector<std::uint8_t> v_bson;
            while (file_ifs.read(reinterpret_cast<char*>(std::addressof(tmp)), sizeof(std::uint8_t))) v_bson.push_back(tmp);
            try {
                file_json = json::from_bson(v_bson);
            } catch (json::parse_error &e) {
                std::string message = "Could not parse " + i_file_path + "\n\twhat(): " + e.what();
                std::string function = __PRETTY_FUNCTION__;
                this->alert(function, message);
            }
        }
    }
    return file_json;
}

std::vector<std::string> DBHandler::split(std::string str, char del) {
    std::size_t first = 0;
    std::size_t last = str.find_first_of(del);
 
    std::vector<std::string> result;
 
    while (first < str.size()) {
        std::string subStr(str, first, last - first);
 
        result.push_back(subStr);
 
        first = last + 1;
        last = str.find_first_of(del, first);
 
        if (last == std::string::npos) {
            last = str.size();
        }
    }
 
    return result;
}

void DBHandler::mkdir(std::string i_dir_path) {
    std::string cmd = "mkdir -p "+i_dir_path;
    if (system(cmd.c_str()) < 0) {
        std::string message = "Problem creating "+i_dir_path;
        std::string function = __PRETTY_FUNCTION__;
        this->alert(function, message); return;
    }
}