package main

import "core:fmt"
import "core:strconv"
import "core:strings"

/* ========================================================================= *
 * 1. LEXER (TOKENIZER)
 * ========================================================================= */

TokenType :: enum {
	EOF = 0,
	CREATE,
	TABLE,
	INT,
	VARCHAR,
	PRIMARY,
	KEY,
	IDENTIFIER,
	NUMBER,
	LPAREN,     // (
	RPAREN,     // )
	COMMA,      // ,
	SEMICOLON,  // ;
	ERROR,
}

Token :: struct {
	type: TokenType,
	text: string,
}

Lexer :: struct {
	source: string,
	cursor: int,
}

lexer_init :: proc(lexer: ^Lexer, source: string) {
	lexer.source = source
	lexer.cursor = 0
}

lexer_peek :: proc(lexer: ^Lexer) -> u8 {
	if lexer.cursor >= len(lexer.source) do return 0
	return lexer.source[lexer.cursor]
}

lexer_advance :: proc(lexer: ^Lexer) -> u8 {
	if lexer.cursor >= len(lexer.source) do return 0
	c := lexer.source[lexer.cursor]
	lexer.cursor += 1
	return c
}

is_whitespace :: proc(c: u8) -> bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
}

is_digit :: proc(c: u8) -> bool {
	return c >= '0' && c <= '9'
}

is_alpha :: proc(c: u8) -> bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
}

is_alnum :: proc(c: u8) -> bool {
	return is_alpha(c) || is_digit(c)
}

lexer_skip_whitespace :: proc(lexer: ^Lexer) {
	for is_whitespace(lexer_peek(lexer)) {
		lexer_advance(lexer)
	}
}

check_keyword_or_ident :: proc(text: string) -> TokenType {
	if strings.equal_fold(text, "CREATE")  do return .CREATE
	if strings.equal_fold(text, "TABLE")   do return .TABLE
	if strings.equal_fold(text, "INT")     do return .INT
	if strings.equal_fold(text, "VARCHAR") do return .VARCHAR
	if strings.equal_fold(text, "PRIMARY") do return .PRIMARY
	if strings.equal_fold(text, "KEY")     do return .KEY
	return .IDENTIFIER
}

lexer_next_token :: proc(lexer: ^Lexer) -> Token {
	lexer_skip_whitespace(lexer)

	start_cursor := lexer.cursor
	c := lexer_advance(lexer)

	if c == 0 {
		return Token{type = .EOF, text = ""}
	}

	switch c {
	case '(': return Token{type = .LPAREN,    text = lexer.source[start_cursor:lexer.cursor]}
	case ')': return Token{type = .RPAREN,    text = lexer.source[start_cursor:lexer.cursor]}
	case ',': return Token{type = .COMMA,     text = lexer.source[start_cursor:lexer.cursor]}
	case ';': return Token{type = .SEMICOLON, text = lexer.source[start_cursor:lexer.cursor]}
	}

	if is_digit(c) {
		for is_digit(lexer_peek(lexer)) {
			lexer_advance(lexer)
		}
		return Token{type = .NUMBER, text = lexer.source[start_cursor:lexer.cursor]}
	}

	if is_alpha(c) || c == '_' {
		for is_alnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_' {
			lexer_advance(lexer)
		}
		text := lexer.source[start_cursor:lexer.cursor]
		type := check_keyword_or_ident(text)
		return Token{type = type, text = text}
	}

	return Token{type = .ERROR, text = lexer.source[start_cursor:lexer.cursor]}
}

/* ========================================================================= *
 * 2. ABSTRACT SYNTAX TREE (AST) DEFINITIONS
 * ========================================================================= */

DataType :: enum {
	INT,
	VARCHAR,
}

ColumnDef :: struct {
	name:           string,
	type:           DataType,
	varchar_length: int,
	is_primary_key: bool,
}

CreateTableStmt :: struct {
	table_name: string,
	columns:    [dynamic]ColumnDef,
}

/* ========================================================================= *
 * 3. RECURSIVE DESCENT PARSER
 * ========================================================================= */

Parser :: struct {
	lexer:   ^Lexer,
	current: Token,
	peek:    Token,
}

parser_advance :: proc(parser: ^Parser) {
	parser.current = parser.peek
	parser.peek = lexer_next_token(parser.lexer)
}

parser_init :: proc(parser: ^Parser, lexer: ^Lexer) {
	parser.lexer = lexer
	parser.current = lexer_next_token(lexer)
	parser.peek = lexer_next_token(lexer)
}

parser_match :: proc(parser: ^Parser, type: TokenType) -> bool {
	if parser.current.type == type {
		parser_advance(parser)
		return true
	}
	return false
}

parser_expect :: proc(parser: ^Parser, type: TokenType, err_msg: string) -> bool {
	if parser.current.type == type {
		parser_advance(parser)
		return true
	}
	fmt.eprintf("Syntax Error: %s. Got '%s'\n", err_msg, parser.current.text)
	return false
}

parse_identifier :: proc(parser: ^Parser) -> (string, bool) {
	if parser.current.type == .IDENTIFIER ||
	   parser.current.type == .TABLE      ||
	   parser.current.type == .KEY {
		id := strings.clone(parser.current.text)
		parser_advance(parser)
		return id, true
	}
	fmt.eprintf("Syntax Error: Expected identifier, got '%s'\n", parser.current.text)
	return "", false
}

parse_column_def :: proc(parser: ^Parser) -> (col: ColumnDef, ok: bool) {
	col_name, id_ok := parse_identifier(parser)
	if !id_ok do return col, false

	col.name = col_name
	col.varchar_length = 255

	if parser_match(parser, .INT) {
		col.type = .INT
	} else if parser_match(parser, .VARCHAR) {
		col.type = .VARCHAR
		if parser_match(parser, .LPAREN) {
			if parser.current.type == .NUMBER {
				val, parse_ok := strconv.parse_int(parser.current.text)
				if parse_ok do col.varchar_length = val
				parser_advance(parser)
			} else {
				fmt.eprintf("Syntax Error: Expected number inside VARCHAR(...)\n")
				delete(col.name)
				return col, false
			}
			if !parser_expect(parser, .RPAREN, "Expected ')' after VARCHAR length") {
				delete(col.name)
				return col, false
			}
		}
	} else {
		fmt.eprintf("Syntax Error: Expected data type for column '%s'\n", col_name)
		delete(col.name)
		return col, false
	}

	if parser_match(parser, .PRIMARY) {
		if !parser_expect(parser, .KEY, "Expected 'KEY' after 'PRIMARY'") {
			delete(col.name)
			return col, false
		}
		col.is_primary_key = true
	}

	return col, true
}

parse_create_table :: proc(parser: ^Parser) -> ^CreateTableStmt {
	if !parser_expect(parser, .CREATE, "Expected 'CREATE'") do return nil
	if !parser_expect(parser, .TABLE, "Expected 'TABLE'")   do return nil

	table_name, ok := parse_identifier(parser)
	if !ok do return nil

	if !parser_expect(parser, .LPAREN, "Expected '(' after table name") {
		delete(table_name)
		return nil
	}

	stmt := new(CreateTableStmt)
	stmt.table_name = table_name
	stmt.columns = make([dynamic]ColumnDef)

	for parser.current.type != .RPAREN && parser.current.type != .EOF {
		col, col_ok := parse_column_def(parser)
		if !col_ok {
			free_create_table_stmt(stmt)
			return nil
		}

		append(&stmt.columns, col)

		if !parser_match(parser, .COMMA) {
			break
		}
	}

	if !parser_expect(parser, .RPAREN, "Expected ')' after column list") {
		free_create_table_stmt(stmt)
		return nil
	}

	parser_match(parser, .SEMICOLON)
	return stmt
}

/* ========================================================================= *
 * 4. CLEANUP & DEBUG PRINTING
 * ========================================================================= */

free_create_table_stmt :: proc(stmt: ^CreateTableStmt) {
	if stmt == nil do return
	delete(stmt.table_name)
	for col in stmt.columns {
		delete(col.name)
	}
	delete(stmt.columns)
	free(stmt)
}

print_ast :: proc(stmt: ^CreateTableStmt) {
	if stmt == nil do return
	fmt.println("CreateTableStmt:")
	fmt.printf("  Table Name: %s\n", stmt.table_name)
	fmt.printf("  Columns (%d):\n", len(stmt.columns))

	for col, idx in stmt.columns {
		type_str := col.type == .INT ? "INT" : "VARCHAR"
		pk_str   := col.is_primary_key ? "YES" : "NO"
		fmt.printf("    [%d] Name: %-8s | Type: %-7s | PrimaryKey: %s\n",
			idx + 1,
			col.name,
			type_str,
			pk_str,
		)
	}
}

/* ========================================================================= *
 * 5. MAIN
 * ========================================================================= */

main :: proc() {
	query := "CREATE TABLE table (id INT PRIMARY KEY, name VARCHAR, did VARCHAR, " +
		"dep VARCHAR, salary INT, city VARCHAR);"

	fmt.printf("Input Query:\n%s\n\n", query)

	lexer: Lexer
	lexer_init(&lexer, query)

	parser: Parser
	parser_init(&parser, &lexer)

	stmt := parse_create_table(&parser)
	if stmt != nil {
		fmt.println("AST successfully constructed:")
		print_ast(stmt)
		free_create_table_stmt(stmt)
	} else {
		fmt.println("Failed to parse query.")
	}
}
