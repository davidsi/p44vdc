//
//  sqlite3persistence.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 13.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__sqlite3persistence__
#define __p44bridged__sqlite3persistence__

#include "sqlite3pp/sqlite3pp.h"

using namespace std;

namespace p44 {

  class SQLite3Persistence : public sqlite3pp::database
  {
    typedef sqlite3pp::database inherited;
  protected:
    /// Get DB Schema upgrade SQL statements
    /// @param aFromVersion current version (0=no database)
    /// @param aToVersion input: desired version, output: version that is generated by returned SQL
    /// @return SQL statements needed to get to aToVersion, empty string if no migration is possible  
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  public:
    /// connects to DB, and performs initialisation/migration as needed to be compatible with the given version
    /// @param aDatabaseFileName the SQLite3 database file path
    /// @param aNeededSchemaVersion the schema version needed to use the DB
    /// @param returns SQLITE_OK or SQLite error code
    int connectAndInitialize(const char *aDatabaseFileName, int aNeededSchemaVersion);
  };

}


#endif /* defined(__p44bridged__sqlite3persistence__) */