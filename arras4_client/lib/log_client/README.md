### Arras Log Client Example 
Below is a sample code that uses the log_client library: 

Please see more examples in unittest/TestLogClient.cc

```c++

...

#include <log_client/LogClient.h>
...

//
// example using sessionId and pageIdx
//
{
    // log client
    arras4::log_client::LogClient logClient("gld", "prod");

    // init pageIdx to 0(first page)
    int pageIdx = 0, recordsPerPage = 20;
    size_t total = 0;
    while (1) {
        std::unique_ptr<arras4::log_client::LogRecords> result;
        try {
            result = logClient->getLogsBySessionId(sId, "", "", pageIdx, recordsPerPage);

        } catch (arras4::log_client::LogClientException &e) {

            std::string msg("Error TestLogClient::testGetBySessionId(): ");
            msg += e.what();
            // some error handling stuff....
        }

        // get results
        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;

        for (size_t i = 0; i < records.size(); i++) {
            const std::string& line = records[i].getLogLine();
            std::cout << line << "\n";
        }

        result.reset(nullptr);

        if (numRecords == 0) 
            break;       // no more records...
        else
            pageIdx ++;  // ask for the next page
    }
}


//
// example using sessionId and start-time
//
{
    // log client
    arras4::log_client::LogClient logClient("gld", "prod");

    // get logs for a given session
    const std::string& sId = "43d00a77-1a73-b0bb-0cb8-0b762fe9aa5f"; // just an example
    std::string start, last;
    size_t total = 0;
    while (1) {

        // start time is empty at the beginning to get the first few records
        std::unique_ptr<arras4::log_client::LogRecords> result = 
            logClient.getLogsBySessionId(sId, start);

        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;

        for (size_t i = 0; i < numRecords; i++) {

            const std::string& line = records[i].getLogLine();
            std::cout << line << "\n";

            last = records[i].timestamp;  // get time stamp
        }

        result.reset(nullptr);

        // if no more records ...
        if (numRecords == 0) 
            break;

        start = last;  // update start time
        // please check timestamp DTZ format : should be "+hh:mm" or "-hh:mm", or "Z"
        
    }
	std::cout << "total number of records: " << total << "\n";
}


```
