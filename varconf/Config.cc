/*
 *  Config.cc - implementation of main configuration class
 *
 *  Copyright (C) 2000, The WorldForge Project
 */

#include <string>
#include <iostream>
#include <fstream>
#include "Config.h"

extern char **environ;

using namespace std;
using namespace SigC;

namespace {
  enum state_t {
    S_EXPECT_NAME, // Expect the start of a name/section/comment
    S_SECTION, // Parsing a section name
    S_NAME, // Parsing an item name
    S_COMMENT, // Parsing a comment
    S_EXPECT_EQ, // Expect an equal sign
    S_EXPECT_VALUE, // Expect the start of a value
    S_VALUE, // Parsing a value
    S_QUOTED_VALUE, // Parsing a "quoted" value
    S_EXPECT_EOL // Expect the end of the line
  };

  enum ctype_t {
    C_SPACE, // Whitespace
    C_NUMERIC, // 0-9
    C_ALPHA, // a-z, A-Z
    C_DASH, // '-' and '_'
    C_EQ, // '='
    C_QUOTE, // '"'
    C_SQUARE_OPEN, // '['
    C_SQUARE_CLOSE, // ']'
    C_HASH, // '#'
    C_ESCAPE, // '\' (an "escape")
    C_EOL, // End of the line
    C_OTHER // Anything else
  };

  ctype_t ctype(char c)
  {
    if (c=='\n') return C_EOL;
    if (isspace(c)) return C_SPACE;
    if (((c >= 'a') && (c <= 'z') || (c >= 'A') && (c <= 'Z'))) return C_ALPHA;
    if (isdigit(c)) return C_NUMERIC;
    if ((c == '-') || (c == '_')) return C_DASH;
    if (c == '=') return C_EQ;
    if (c == '"') return C_QUOTE;
    if (c == '[') return C_SQUARE_OPEN;
    if (c == ']') return C_SQUARE_CLOSE;
    if (c == '#') return C_HASH;
    if (c == '\\') return C_ESCAPE;
    return C_OTHER;
  }
}

namespace varconf {

Config* Config::m_instance;

Config::Config()
{
}

Config* Config::inst()
{
  if ( m_instance == NULL) 
    m_instance = new Config;
  
  return m_instance;
}

Variable Config::getItem( const string& section, const string& name)
{
  return ( m_conf[section])[name];
}


void Config::setItem( const string& section, const string& name,
                      const Variable item)
{
  string sec = section, nam = name;

  if ( nam.empty()) {
    throw "Invalid configuration item!";
  }
  
  for ( size_t i = 0; i < sec.size(); i++) {
    ctype_t c = ctype( sec[i]);
    sec[i] = tolower( sec[i]);

    if ( ( c != C_NUMERIC) && ( c != C_ALPHA) && ( c != C_DASH)) {
      sec[i] = '_';
    }
  }

  for ( size_t i = 0; i < nam.size(); i++) {
    ctype_t c = ctype( nam[i]);
    nam[i] = tolower( nam[i]);

    if ( (c != C_NUMERIC) && ( c != C_ALPHA) && ( c != C_DASH)) {
      nam[i] = '_';
    }
  }
 
  ( m_conf[sec])[nam] = item;
 
  sig.emit(); 
  sigv.emit( section, name);

} // Config::setItem()


bool Config::findItem( const string& section, const string& name)
{
  return ( (m_conf.count( section)) && ( m_conf[section].count( name)));
}


bool Config::readFromFile( const string& filename)
{
  ifstream fin( filename.c_str());
  
  if ( fin.fail()) {
    cerr << "Could not open " << filename << " for input!\n";
    return false;
  }
  try {
    parseStream( fin);
  }
  catch ( ParseError p) {
    cerr << "While parsing " << filename << ":\n";
    cerr << p;
  }
  
  return true;
}


bool Config::writeToFile( const string& filename)
{
  ofstream fout( filename.c_str());

  if ( fout.fail()) {
    cerr << "Could not open " << filename << " for output!\n";
    return false;
  }

  return writeToStream( fout);
}


bool Config::writeToStream( ostream& out)
{
  conf_map::iterator I;
  sec_map::iterator J;
 
  for ( I = m_conf.begin(); I != m_conf.end(); I++) {
    out << endl 
        << "[" << ( *I).first << "]\n\n";
    
    for ( J = ( *I).second.begin(); J != ( *I).second.end(); J++) {
      out << ( *J).first << " = \"" << ( *J).second << "\"\n";
    }
  }
  
  return true;
}


void Config::parseStream( istream& in) throw ( ParseError)
{
  char c; 
  bool escaped = false;
  size_t line = 1, col = 0;
  string name = "", value = "", section = "";
  state_t state = S_EXPECT_NAME;

  while ( in.get( c)) {
    col++;
    switch ( state) {
      case S_EXPECT_NAME : 
	switch ( ctype( c)) {
	  case C_ALPHA:
	  case C_NUMERIC:
	  case C_DASH:
	    state = S_NAME;
	    name = c;
	    break;
	  case C_SQUARE_OPEN:
            section = "";
	    state = S_SECTION;
	    break;
	  case C_SPACE:
	  case C_EOL:
	    break;
	  case C_HASH:
	    state = S_COMMENT;
	    break;
	  default:
	    throw ParseError( "item name", line, col);
	    break;
	}
	break;
      case S_SECTION :
	switch ( ctype( c)) {
	  case C_ALPHA:
	  case C_NUMERIC:
	    section += c;
	    break;
	  case C_SQUARE_CLOSE:
	    state = S_EXPECT_EOL;
	    break;
	  default:
	    throw ParseError( "']'", line, col);
	    break;
	}
        break;
      case S_NAME :
	switch (ctype( c)) {
	  case C_ALPHA:
	  case C_NUMERIC:
	  case C_DASH:
	    name += c;
	    break;
	  case C_EQ:
	    state = S_EXPECT_VALUE;
	    break;
	  case C_SPACE:
	    state = S_EXPECT_EQ;
	    break;
	  default:
	    throw ParseError( "'='", line, col);
	    break;
	}
	break;
      case S_COMMENT :
	switch ( ctype( c)) {
	  case C_EOL:
	    state = S_EXPECT_NAME;
	    break;
	  default:
	    break;
        }
	break;
      case S_EXPECT_EQ:
	switch ( ctype( c)) {
	  case C_SPACE:
	    break;
	  case C_EQ:
	    state = S_EXPECT_VALUE;
	    break;
	  default:
	    throw ParseError( "'='", line, col);
	    break;
	}
	break;
      case S_EXPECT_VALUE:
	switch ( ctype( c)) {
	  case C_ALPHA:
	  case C_NUMERIC:
	  case C_DASH:
	    state = S_VALUE;
	    value = c;
	    break;
	  case C_QUOTE:
            value = "";
	    state = S_QUOTED_VALUE;
	    break;
	  case C_SPACE:
	    break;
	  default:
	    throw ParseError( "value", line, col);
	    break;
	}
	break;
      case S_VALUE:
	switch ( ctype( c)) {
	  case C_QUOTE:
	    throw ParseError( "value", line, col);
	  case C_SPACE:
	    state = S_EXPECT_EOL;
	    setItem( section, name, value);
	    break;
	  case C_EOL:
	    state = S_EXPECT_NAME;
	    setItem( section, name, value);
	    break;
	  case C_HASH:
	    state = S_COMMENT;
	    setItem( section, name, value);
	    break;
	  default:
	    value += c;
	    break;
	}
	break;
      case S_QUOTED_VALUE:
        if ( escaped) {
          value += c;
	  escaped = false;
        } else {
	  switch ( ctype( c)) {
	    case C_QUOTE:
	      state = S_EXPECT_EOL;
	      setItem( section, name, value);
	      break;
	    case C_ESCAPE:
	      escaped = true;
	      break;
	    default:
	      value += c;
	      break;
	  }
	}
	break;
      case S_EXPECT_EOL:
	switch ( ctype( c)) {
	  case C_HASH:
	    state = S_COMMENT;
	    break;
	  case C_EOL:
	    state = S_EXPECT_NAME;
	    break;
	  case C_SPACE:
	    break;
	  default:
	    throw ParseError( "end of line", line, col);
            break;
	}
	break;
      default:
        break;
    }
    if ( c == '\n') {
      line++;
      col = 0;
    }
  } // while ( in.get( c))

  if ( state == S_QUOTED_VALUE) {
    throw ParseError( "\"", line, col);
  }

  if ( state == S_VALUE) {
    setItem( section, name, value);
  }
} // Config::parseStream()


void Config::setParameterLookup( char short_form, const string& long_form,
                                 bool needs_value = true) 
{
    m_par_lookup[short_form] = pair<string, bool>( long_form, needs_value);  
}


void Config::getCmdline( int argc, char** argv)
{
  string name = "", value = "", section = "";
 
  for ( int i = 1; i < argc; i++) {
    if ( argv[i][1] == '-' && argv[i][0] == '-') {
      string arg( argv[i]);
      size_t eq_pos = arg.find('=');
      size_t col_pos = arg.find(':');
 
      if ( col_pos != string::npos && eq_pos != string::npos) {
        if ( ( eq_pos - col_pos) > 1) {
          section = arg.substr( 2, ( col_pos - 2));
          name = arg.substr( ( col_pos + 1), ( eq_pos - ( col_pos + 1)));
          value = arg.substr( ( eq_pos + 1), ( arg.size() - ( eq_pos + 1)));
        }
        else {
          section = "";
          name = arg.substr( 2, ( eq_pos - 2));
          value = arg.substr( ( eq_pos + 1), ( arg.size() - ( eq_pos + 1)));
        }
      } 
      else if ( col_pos != string::npos && eq_pos == string::npos) {
        section = arg.substr( 2, ( col_pos - 2));
        name = arg.substr( ( col_pos + 1), ( arg.size() - ( col_pos + 1)));
        value = "";
      }
      else if ( col_pos == string::npos && eq_pos != string::npos) {
        section = "";
        name = arg.substr( 2, ( eq_pos - 2));
        value = arg.substr( ( eq_pos + 1), ( arg.size() - ( eq_pos + 1)));
      }
      else {
        section = "";
        name = arg.substr( 2, ( arg.size() - 2));
        value = "";
      } 

      setItem( section, name, value);

    } // argv[i][1] == '-' && argv[i][0] == '-'
    else if ( argv[i][1] != '-' && argv[i][0] == '-')  {
      parameter_map::iterator I = m_par_lookup.find( argv[i][1]);
        
      if ( I != m_par_lookup.end()) {
        section = ""; 
        name = ( ( *I).second).first;

        if ( ( ( *I).second).second) {
          if ( argv[i+1] != NULL && argv[i+1][0] != '-') {
            value = argv[++i];

            setItem( section, name, value); 
          }         
        }
      }
    } // argv[i][0] == '-'
  }
} // Config::getCmdline()
 

void Config::getEnv(const string& prefix)
{
  string name = "", value = "", section = "", env = "";
  size_t eq_pos = 0;

  for ( int i = 0; environ[i] != NULL; i++) {
    env = environ[i];

    if ( env.substr( 0, prefix.size()) == prefix) {
      section = name = value = "";
      eq_pos = env.find( '='); 

      if ( eq_pos != string::npos) {
        value = env.substr( ( eq_pos + 1), ( env.size() - ( eq_pos + 1)));
      }
      else {
        value = "";
      }
      
      name = env.substr( prefix.size(), ( eq_pos - prefix.size()));
      if ( !name.empty() ) {
        setItem( section, name, value);
      }
      else {
        throw "Invalid environment setting!";
      }
    }
  }
} // Config::getEnv()


} // namespace varconf
