#include "catch.hpp"
#include "test_helpers.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test prepared statements API", "[api]") {
	unique_ptr<QueryResult> result;
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE a (i TINYINT)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO a VALUES (11), (12), (13)"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE strings(s VARCHAR)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO strings VALUES (NULL), ('test')"));

	// query using a prepared statement
	// integer:
	result = con.Query("SELECT COUNT(*) FROM a WHERE i=$1", 12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	// strings:
	result = con.Query("SELECT COUNT(*) FROM strings WHERE s=$1", "test");
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	// multiple parameters
	result = con.Query("SELECT COUNT(*) FROM a WHERE i>$1 AND i<$2", 10, 13);
	REQUIRE(CHECK_COLUMN(result, 0, {2}));

	// test various integer types
	result = con.Query("SELECT COUNT(*) FROM a WHERE i=$1", (int8_t) 12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	result = con.Query("SELECT COUNT(*) FROM a WHERE i=$1", (int16_t) 12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	result = con.Query("SELECT COUNT(*) FROM a WHERE i=$1", (int32_t) 12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	result = con.Query("SELECT COUNT(*) FROM a WHERE i=$1", (int64_t) 12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));

	// create a prepared statement and use it to query
	auto prepare = con.Prepare("SELECT COUNT(*) FROM a WHERE i=$1");

	result = prepare->Execute(12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	result = prepare->Execute(13);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));

	string prepare_name = prepare->name;
	// we can execute the prepared statement ourselves as well using the name
	result = con.Query("EXECUTE " + prepare_name + "(12)");
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	// if we destroy the prepared statement it goes away
	prepare.reset();
	REQUIRE_FAIL(con.Query("EXECUTE " + prepare_name + "(12)"));
}

TEST_CASE("Test destructors of prepared statements", "[api]") {
	unique_ptr<DuckDB> db;
	unique_ptr<Connection> con;
	unique_ptr<PreparedStatement> prepare;
	unique_ptr<QueryResult> result;

	// test destruction of connection
	db = make_unique<DuckDB>(nullptr);
	con = make_unique<Connection>(*db);
	// create a prepared statement
	prepare = con->Prepare("SELECT $1::INTEGER+$2::INTEGER");
	// we can execute it
	result = prepare->Execute(3, 5);
	REQUIRE(CHECK_COLUMN(result, 0, {8}));
	// now destroy the connection
	con.reset();
	// now we can't use the prepared statement anymore
	REQUIRE_FAIL(prepare->Execute(3, 5));
	// destroying the prepared statement is fine
	prepare.reset();

	// test destruction of db
	// create a connection and prepared statement again
	con = make_unique<Connection>(*db);
	prepare = con->Prepare("SELECT $1::INTEGER+$2::INTEGER");
	// we can execute it
	result = prepare->Execute(3, 5);
	REQUIRE(CHECK_COLUMN(result, 0, {8}));
	// destroy the db
	db.reset();
	// now we can't use the prepared statement anymore
	REQUIRE_FAIL(prepare->Execute(3, 5));
	// neither can we use the connection
	REQUIRE_FAIL(con->Query("SELECT 42"));
	// or prepare new statements
	prepare = con->Prepare("SELECT $1::INTEGER+$2::INTEGER");
	REQUIRE(!prepare->success);
}

TEST_CASE("Test incorrect usage of prepared statements API", "[api]") {
	unique_ptr<QueryResult> result;
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE a (i TINYINT)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO a VALUES (11), (12), (13)"));

	// this fails if there is a mismatch between number of arguments in prepare and in variadic
	// too few:
	REQUIRE_FAIL(con.Query("SELECT COUNT(*) FROM a WHERE i=$1 AND i>$2", 11));
	// too many:
	REQUIRE_FAIL(con.Query("SELECT COUNT(*) FROM a WHERE i=$1 AND i>$2", 11, 13, 17));

	// prepare an SQL string with a parse error
	auto prepare = con.Prepare("SELEC COUNT(*) FROM a WHERE i=$1");
	// we cannot execute this prepared statement
	REQUIRE_FAIL(prepare->Execute(12));

	// cannot prepare multiple statements at once
	prepare = con.Prepare("SELECT COUNT(*) FROM a WHERE i=$1; SELECT 42+$2;");
	REQUIRE_FAIL(prepare->Execute(12));

	// also not in the Query syntax
	REQUIRE_FAIL(con.Query("SELECT COUNT(*) FROM a WHERE i=$1; SELECT 42+$2", 11));
}

TEST_CASE("Test multiple prepared statements", "[api]") {
	unique_ptr<QueryResult> result;
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE a (i TINYINT)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO a VALUES (11), (12), (13)"));

	// test that we can have multiple open prepared statements at a time
	auto prepare = con.Prepare("SELECT COUNT(*) FROM a WHERE i=$1");
	auto prepare2 = con.Prepare("SELECT COUNT(*) FROM a WHERE i>$1");

	result = prepare->Execute(12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	result = prepare2->Execute(11);
	REQUIRE(CHECK_COLUMN(result, 0, {2}));
}

TEST_CASE("Test prepared statements and transactions", "[api]") {
	unique_ptr<QueryResult> result;
	DuckDB db(nullptr);
	Connection con(db);

	// create prepared statements in a transaction
	REQUIRE_NO_FAIL(con.Query("BEGIN TRANSACTION"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE a (i TINYINT)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO a VALUES (11), (12), (13)"));

	auto prepare = con.Prepare("SELECT COUNT(*) FROM a WHERE i=$1");
	auto prepare2 = con.Prepare("SELECT COUNT(*) FROM a WHERE i>$1");

	result = prepare->Execute(12);
	REQUIRE(CHECK_COLUMN(result, 0, {1}));
	result = prepare2->Execute(11);
	REQUIRE(CHECK_COLUMN(result, 0, {2}));
	// now if we rollback our prepared statements are invalidated
	REQUIRE_NO_FAIL(con.Query("ROLLBACK"));

	REQUIRE_FAIL(prepare->Execute(12));
	REQUIRE_FAIL(prepare2->Execute(11));
}
