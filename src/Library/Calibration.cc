#include <stdexcept>
#include <assert.h>
#include <iostream>
#include <memory>

#include "CCDB/Calibration.h"
#include "CCDB/GlobalMutex.h"
#include "CCDB/Providers/DataProvider.h"
#include "CCDB/Helpers/PathUtils.h"
#include "CCDB/Helpers/TimeProvider.h"
#include "CCDB/Helpers/PerfLog.h"

using namespace std;

namespace ccdb
{

//______________________________________________________________________________
Calibration::Calibration()
{
    //Constructor 

    mProvider = NULL;
    mProviderIsLocked = false; //by default we assume that we own the provider
    mDefaultRun = 0;
	mDefaultTime = 0;
    mDefaultVariation = "default";
    mIsAutoReconnect = true;
    mLastActivityTime=0;

#ifdef CCDB_CACHE_ON
    mIsCacheEnabled = true;
#else
    mIsCacheEnabled = false;
#endif
}


//______________________________________________________________________________
Calibration::Calibration(int defaultRun, string defaultVariation/*="default"*/, time_t defaultTime/*=0*/ )
{	
    //Constructor 

	mDefaultRun = defaultRun;
	mDefaultVariation = defaultVariation;
	mDefaultTime = defaultTime;

    mProvider = NULL;
    mProviderIsLocked = false;      // by default we assume that we own the provider
    PthreadSyncObject * x = NULL;
    x = new PthreadSyncObject();
    mIsAutoReconnect = true;
    mLastActivityTime=0;

#ifdef CCDB_CACHE_ON
    mIsCacheEnabled = true;
#else
    mIsCacheEnabled = false;
#endif
}


//______________________________________________________________________________
Calibration::~Calibration()
{
    //Destructor
    if(!mProviderIsLocked && mProvider!=NULL) delete mProvider;
}


//______________________________________________________________________________
void Calibration::UseProvider( DataProvider * provider, bool lockProvider/*=true*/ )
{
    // set provider to use. 
    //if lockProvider==true, than @see Connect, @see Disconnect and @see SetConnectionString 
    //will not affect connection of provider. The provider will not be deleted in destruction. 
    Lock();
	mProvider = provider;	
	mProviderIsLocked = lockProvider;
    Unlock();
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector< map<string, string> > &values, const string & namepath )
{
     /** @brief Get constants by namepath
     * 
     * This version of function fills values as a Table, 
     * vector<map<string, string>> - is vector of rows, each row is a map<header_name, string_cell_value>
     *
     * Namepath is usually is just the data path, like 
     * /path/to/data
     * but the full form of request is 
     * /path/to/data:run:variation:time
     * request might be shortened. One may skip 
     * /path/to/data - just path to date, no run, variation and timestamp specified
     * /path/to/data::mc - no run or date specified.
     * /path/to/data:::2029 - only path and date
     * 
     * 
	 * @parameter [out] values - vector of rows, each row is a map<header_name, string_cell_value>
	 * @parameter [in]  namepath - data path
	 * @return true if constants were found and filled. false if namepath was not found. raises std::logic_error if any other error acured.
	 */  

    auto assignment = GetAssignment(namepath, true);
        
    if(!assignment)
    {       
        return false; //TODO possibly exception throwing?
    }

    //Get data
    
    assert(values.empty());
    
    assignment->GetMappedData(values);
    
    //check data, get columns 
    if(values.size() == 0){
        throw std::logic_error("Calibration::GetCalib( vector< map<string, string> >&, const string&). Data has no rows. Zero rows are not supposed to be.");
    }
    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector< map<string, double> > &values, const string & namepath )
{
    //read values

    //TODO right now the function works through copy. 
    //one needs to find out, maybe it needs to be reimplemented
    
    vector<map<string, string> > rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
    	throw;
    }

    assert(values.empty());
    
    int rowsNum = rawValues.size();
    int columnsNum = rawValues[0].size(); //rawTableValues[0] - size was checked in GetCalib

    values.resize(rawValues.size());

    //compose values
    for (int rowIter = 0; rowIter < rowsNum; rowIter++)
    {
        map<string, string>::iterator iter;
        for ( iter=rawValues[rowIter].begin() ; iter != rawValues[rowIter].end(); iter++ )
        {
            double tmpVal = StringUtils::ParseDouble(iter->second);
            values[rowIter][iter->first] = tmpVal;
        }
    }

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector< map<string, int> > &values, const string & namepath )
{
    //TODO right now the function works through copy. 
    //one needs to find out, maybe it needs to be reimplemented

    vector<map<string, string> > rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }

    assert(values.empty());

    int rowsNum = rawValues.size();
    int columnsNum = rawValues[0].size(); //rawTableValues[0] - size was checked in GetCalib

    values.resize(rawValues.size());

    //compose values
    for (int rowIter = 0; rowIter < rowsNum; rowIter++)
    {
        map<string, string>::iterator iter;
        for ( iter=rawValues[rowIter].begin() ; iter != rawValues[rowIter].end(); iter++ )
        {
            int intlVal = StringUtils::ParseInt(iter->second);
            values[rowIter][iter->first] = intlVal;
        }
    }

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector< vector<string> > &values, const string & namepath )
{
    /** @brief Get constants by namepath
     * 
     * this version of function fills values as a table represented as
     * vector< vector<string> > = vector of rows where each row is a vector of cells
     *
     * @parameter [out] values - vector of rows, each row is a map<header_name, string_cell_value>
     * @parameter [in]  namepath - data path
     * @return true if constants were found and filled. false if namepath was not found. raises std::logic_error if any other error acured.
     */
    
    auto assignment = GetAssignment(namepath, false);
    
    if(!assignment)
    {
        return false;
    }
   
    assignment->GetData(values);
    
    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector< vector<double> > &values, const string & namepath )
{
    //TODO right now the function works through copy. One has to check, maybe it needs to be reimplemented

    vector< vector<string> > rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }

    assert(values.empty());

    int rowsNum = rawValues.size();
    int columnsNum = rawValues[0].size(); //rawTableValues[0] - size was checked in GetCalib

    values.resize(rawValues.size());

    //compose values
    for (int rowIter = 0; rowIter < rowsNum; rowIter++)
    {   
        for (int columnsIter = 0; columnsIter < columnsNum; columnsIter++)
        {
            double tmpVal = StringUtils::ParseDouble(rawValues[rowIter][columnsIter]);
            values[rowIter].push_back(tmpVal);
        }
    }

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector< vector<int> > &values, const string & namepath )
{
    vector< vector<string> > rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }

    assert(values.empty());

    int rowsNum = rawValues.size();
    int columnsNum = rawValues[0].size(); //rawTableValues[0] - size was checked in GetCalib

    values.resize(rawValues.size());

    //compose values
    for (int rowIter = 0; rowIter < rowsNum; rowIter++)
    {   
        for (int columnsIter = 0; columnsIter < columnsNum; columnsIter++)
        {
            int tmpVal = StringUtils::ParseInt(rawValues[rowIter][columnsIter]);
            values[rowIter].push_back(tmpVal);
        }
    }

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( map<string, string> &values, const string & namepath )
{
     /** @brief Get constants by namepath
     * 
     * this version of function fills values as one row
     * where map<header_name, string_cell_value>
     *
     * @parameter [out] values - as  map<header_name, string_cell_value>
     * @parameter [in]  namepath - data path
     * @return true if constants were found and filled. false if namepath was not found. raises std::logic_error if any other error acured.
     */


    auto assignment = GetAssignment(namepath, true);
    
    if(assignment == NULL)
    {
        //TODO possibly exception throwing?
        return false;
    }

    //Get data
    vector< vector<string> > rawTableValues;
    assignment->GetData(rawTableValues);

    //check data a little...
    if(rawTableValues.size() == 0)
    {
        throw std::logic_error("Calibration::GetCalib( map<string, string>&, const string&). Data has no rows. Zero rows are not supposed to be.");
    }
	
	//VALUES VALIDATION
	assert(values.empty());

	// This method is used to return a 1-D array of values (in the form of a
	// map<string, string>). The data may be stored in either column-wise (1 
	// row with many columns) or row-wise (1 column with many rows). We wish
	// to support either so we must check which format it is in. If it is
	// stored row-wise, then we'll need to make up the column names so that
	// the map being returned is properly ordered.
	// 5/25/2014  D. Lawrence
	
	// Make sure at least one dimension is exactly 1. (Assume all inner vectors
	// are the same size as the zeroth one.)
	int rowsNum = rawTableValues.size();
	int columnsNum = rawTableValues[0].size();
	if(rowsNum>1 && columnsNum>1){
		throw std::logic_error("Calibration::GetCalib( map<string, string>&, const string&). Appears to be a table (both dimensions are > 1).");
	}
	
	if(rowsNum>1){
		// ---- ROW-WISE ----
		
		// Loop over rows, generating a column name for each and filling "values"
		for(unsigned int i=0; i<rowsNum; i++){
			char colName[16];
			sprintf(colName, "v%04d", i); // TODO this will be a problem for more than 10k values!
			values[colName] = rawTableValues[i][0];
		}
		
	}else{
		// ---- COLUMN-WISE ----

		//now we take only first row of table
		vector<string> rawValues = rawTableValues[0];

		//get columns names
		vector<string> columnNames = assignment->GetTypeTable()->GetColumnNames();
		assert(columnsNum == columnNames.size());

		//compose values
		for (int i=0; i<columnsNum; i++) values[columnNames[i]] = rawValues[i];
	}

    //finishing
    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( map<string, double> &values, const string & namepath )
{
    map<string, string> rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }

    assert(values.empty());
    
    //compose values
    map<string, string>::iterator iter;
    for ( iter=rawValues.begin() ; iter != rawValues.end(); iter++ )
    {
        double tmpVal = StringUtils::ParseDouble(iter->second);
        values[iter->first] = tmpVal;
    }

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( map<string, int> &values, const string & namepath )
{
    //TODO right now the function works through copy. 
    //one needs to find out, maybe it needs to be reimplemented


    map<string, string> rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }

    assert(values.empty());

    //compose values
    map<string, string>::iterator iter;
    for ( iter=rawValues.begin() ; iter != rawValues.end(); iter++ )
    {
        int tmpVal = StringUtils::ParseInt(iter->second);
        values[iter->first] = tmpVal;
    }

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector<string> &values, const string & namepath )
{
    /** @brief Get constants by namepath
     * 
     * this version of function fills values as one row as vector<string_values>
     * 
     * @parameter [out] values - as vector<string_values>
     * @parameter [in]  namepath - data path
     * @return true if constants were found and filled. false if namepath was not found. raises std::logic_error if any other error acured.
     */

	
    
	auto assignment = GetAssignment(namepath, true);
    
    if(assignment == NULL) return false; //TODO possibly exception throwing?

    //Get data
    values.clear();
    assignment->GetVectorData(values);
   
    //check data and check that the user will get what he ment...
    if(values.size() == 0)
        throw std::logic_error("Calibration::GetCalib(vector<string> &, const string &). Data has no rows. Zero rows are not supposed to be.");

    if(values.size() != assignment->GetTypeTable()->GetColumnsCount())
        throw std::logic_error("Calibration::GetCalib(vector<string> &, const string &). logic_error: Calling of single row vector<dataType> version of GetCalib method on dataset that has more than one rows. Use GetCalib vector<vector<dataType> > instead.");

    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector<double> &values, const string & namepath )
{
    vector<string> rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }
    
    int columnsNum = rawValues.size(); //rawTableValues[0] - size was checked in GetCalib    

    values.resize(rawValues.size());

    //compose values
    for (int columnsIter = 0; columnsIter < columnsNum; columnsIter++)
    {
        double tmpVal = StringUtils::ParseDouble(rawValues[columnsIter]);
        values[columnsIter] = tmpVal;
    }
    return true;
}


//______________________________________________________________________________
bool Calibration::GetCalib( vector<int> &values, const string & namepath )
{
    vector<string> rawValues;
    try
    {
        if(!GetCalib(rawValues, namepath)) return false;
    }
    catch (std::exception &)
    {
        throw;
    }

    int columnsNum = rawValues.size(); //rawTableValues[0] - size was checked in GetCalib

    values.resize(rawValues.size());

    //compose values
    for (int columnsIter = 0; columnsIter < columnsNum; columnsIter++)
    {
        int tmpVal = StringUtils::ParseInt(rawValues[columnsIter]);
        values[columnsIter] = tmpVal;
    }
    return true;
}

//______________________________________________________________________________
bool Calibration::GetCalib(string &value, const string & namepath)
{
	/** @brief Get constant by namepath
	 *
	 * This version of function fills just one value
	 *
	 * @remark 	Actually the function calls  GetCalib(vector<string> &values, ...)
	 * 			and converts its first element to the required type
	 *
	 * @parameter [out] value
	 * @parameter [in]  namepath - data path
	 * @return true if constants were found and filled. false if namepath was not found. raises std::exception if any other error acured.
	 */

	vector<string> rawValues;
	try
	{
		if(!GetCalib(rawValues, namepath)) return false;
	}
	catch (std::exception &)
	{
		throw;
	}

	assert(rawValues.size());
	value = rawValues[0];
	return true;
}

//______________________________________________________________________________
bool Calibration::GetCalib(double &value, const string & namepath)
{
	string rawValue;
	if(!GetCalib(rawValue, namepath)) return false;
	value = StringUtils::ParseDouble(rawValue);
	return false;
}

//______________________________________________________________________________
bool Calibration::GetCalib(int &value, const string & namepath)
{
	string rawValue;
	if(!GetCalib(rawValue, namepath)) return false;
	value = StringUtils::ParseInt(rawValue);
	return false;
}

//______________________________________________________________________________
string Calibration::GetConnectionString() const
{
    //
    if (mProvider!=NULL) return mProvider->GetConnectionString();
    
    return string();
}


//______________________________________________________________________________
    Assignment* Calibration::GetAssignment(const string& namepath, bool loadColumns /*=true*/)
{
    /** @brief Gets the assignment from provider using namepath
     * namepath is the common ccdb request; @see GetCalib
     *
     * @remark the function is thread safe
     * 
     * @parameter [in] namepath - full namepath is /path/to/data:run:variation:time but usually it is only /path/to/data
     * @return   DAssignment *
     */

    auto pl = PerfLog("Calibration::GetAssignment=>" + namepath );
    static std::map<std::string, Assignment*> cache;

	UpdateActivityTime();

    RequestParseResult result = PathUtils::ParseRequest(namepath);
    string variation = (result.WasParsedVariation ? result.Variation : mDefaultVariation);
    string loadColumnsFlag;
    int run  = (result.WasParsedRunNumber ? result.RunNumber : mDefaultRun);



    auto time = result.WasParsedTime ? result.Time: mDefaultTime;

    CheckConnection();  // Check if is connected and reconnect if needed (and allowed)
	
    //Lock();Unlock();
    std::lock_guard<std::mutex> lock(mReadMutex);

    // Check if we have this value in the cache
    if (loadColumns) {
      loadColumnsFlag = "cols";
    } else {
      loadColumnsFlag = "no_cols";
    }
    string cache_key = namepath + ":" + to_string(run) + ":" + variation + ":" + to_string(time) + ":" + loadColumnsFlag;
    if(mIsCacheEnabled)
    {
        if (  cache.find(cache_key) != cache.end() ) {
            return cache[cache_key];
        }
    }

    Assignment* assigment;

    if(time > 0)
    {
		assigment = (mProvider->GetAssignmentShort(run, PathUtils::MakeAbsolute(result.Path), time, variation,loadColumns));
	}
    else
	{
		assigment = (mProvider->GetAssignmentShort(run, PathUtils::MakeAbsolute(result.Path), variation,loadColumns));
	}

    if(mIsCacheEnabled)
    {

        cache[cache_key] = assigment;
    }

    return assigment;
}


//______________________________________________________________________________
void Calibration::Lock()
{
    //Thread mutex lock for multithreaded operations
    CCDBGlobalMutex::Instance()->ReadConstantsLock();
}


//______________________________________________________________________________
void Calibration::Unlock()
{
    //Thread mutex Unlock lock for multithreaded operations
    CCDBGlobalMutex::Instance()->ReadConstantsRelease();
}


//______________________________________________________________________________
void Calibration::GetListOfNamepaths( vector<string> &namepaths )
{
   /** @brief Get list of all type tables with full path
    *
    * @parameter [in] vector<string> & namepaths
    * @return   void
    */
    
    UpdateActivityTime();

    vector<ConstantsTypeTable*> tables;
    std::lock_guard<std::mutex> lock(mReadMutex);
	 bool ok = mProvider->SearchConstantsTypeTables(tables, "*");

    if(!ok)
    {
        throw logic_error("Error selecting all type tables");
    }

    for (auto &table : tables) {
        // we use substr(1) because JANA users await list
        // without '/' in the beginning of each string,
        // while GetFullPath() returns strings that start with '/'
        namepaths.push_back(table->GetFullPath().substr(1));
        delete table;
    }
}


//______________________________________________________________________________
bool Calibration::Reconnect()
{
   /**
     * @brief Connects to database using the connection string 
     *        of the last @see Connect function call
     *
     * @remark Just returns true if already connected
     * @exception logic_error if Connect hasn't been called before
     * 
     * @return true if connected
     */

    if(IsConnected()) return true;          //How was it? - Just returns true if already connected
    
    string constr = GetConnectionString();

    //Check if we have connection string === connected ever before
    if(constr.length()==0)
    {
        throw std::logic_error("Can not reconnect to database because connection string is empty. Has one connected to DB before REconnect?");
    }

    //Connect...
    return Connect(constr);
}


//______________________________________________________________________________
void Calibration::CheckConnection()
{
    if(!this->IsConnected())
    {
        if(!mIsAutoReconnect) throw std::logic_error("Calibration class is not connected to data source. Auto-reconnection is turned off.");
        this->Reconnect();
    }
}

//______________________________________________________________________________
void Calibration::UpdateActivityTime()
{
    /** @brief Updates time of last database activity
     *
     * @warning (!) function MUST be called on each action that uses database 
     *              (constants read, connection established or reconnection)
     *
     */
    mLastActivityTime = TimeProvider::GetUnixTimeStamp(ClockSources::Monotonic);
}

    /** @brief if true the data will be cached
     *
     * @param value true - enable cache, false - disable
     *
     * @remarks - cache greatly (2 magnitudes) reduses the time to get the same constants from DB
     *            but it costs some memory. Shouldn't be a bug source but caches are alwais caches
     */
    void Calibration::EnableCache(bool value) { mIsCacheEnabled = value;}

    /** @brief if true the caching is using */
    bool Calibration::IsCacheEnabled() { return mIsCacheEnabled;}

}

