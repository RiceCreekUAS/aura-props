#if defined(ARDUPILOT_BUILD)
#  include <AP_Filesystem/AP_Filesystem.h>
#  undef _GLIBCXX_USE_C99_STDIO   // vsnprintf() not defined
#  include "setup_board.h"
#else
#  include <fcntl.h>            // open()
#  include <unistd.h>           // read()
#endif

#include <stdio.h>

#include <vector>
#include <string>
using std::vector;
using std::string;

#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#include "strutils.h"
#include "props2.h"

static void pretty_print_tree(Value *v) {
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    v->Accept(writer);
    //const char* output = buffer.GetString();
    printf("%s\n", buffer.GetString());
}

static bool is_integer(const string val) {
    for ( int i = 0; i < val.length(); i++ ) {
        if ( val[i] < '0' or val[i] > '9' ) {
            return false;
        }
    }
    return true;
}

static bool extend_array(Value *node, int size) {
    if ( !node->IsArray() ) {
        node->SetArray();
    }
    for ( int i = node->Size(); i <= size; i++ ) {
        printf("    extending: %d\n", i);
        Value newobj(kObjectType);
        node->PushBack(newobj, doc.GetAllocator());
    }
    return true;
}

PropertyNode::PropertyNode() {
}

static Value *find_node_from_path(Value *start_node, string path, bool create) {
    Value *node = start_node;
    printf("PropertyNode(%s)\n", path.c_str());
    if ( !node->IsObject() ) {
        node->SetObject();
        if ( !node->IsObject() ) {
            printf("  still not object after setting to object.\n");
        }              
    }
    vector<string> tokens = split(path, "/");
    for ( int i = 0; i < tokens.size(); i++ ) {
        if ( tokens[i].length() == 0 ) {
            continue;
        }
        // printf("  token: %s\n", tokens[i].c_str());
        if ( is_integer(tokens[i]) ) {
            // array reference
            int index = std::stoi(tokens[i].c_str());
            extend_array(node, index+1);
            // printf("Array size: %d\n", node->Size());
            node = &(*node)[index];
            //PropertyNode(node).pretty_print();
        } else {
            if ( node->HasMember(tokens[i].c_str()) ) {
                // printf("    has %s\n", tokens[i].c_str());
                node = &(*node)[tokens[i].c_str()];
            } else if ( create ) {
                printf("    creating %s\n", tokens[i].c_str());
                Value key;
                key.SetString(tokens[i].c_str(), tokens[i].length(), doc.GetAllocator());
                Value newobj(kObjectType);
                node->AddMember(key, newobj, doc.GetAllocator());
                node = &(*node)[tokens[i].c_str()];
                // printf("  new node: %p\n", node);
            } else {
                return nullptr;
            }
        }
    }
    if ( node->IsArray() ) {
        // when node is an array and no index specified, default to /0
        if ( node->Size() > 0 ) {
            node = &(*node)[0];
        }
    }
    // printf(" found/create node->%d\n", (int)node);
    return node;
}

PropertyNode::PropertyNode(string abs_path, bool create) {
    // printf("PropertyNode(%s) %d\n", abs_path.c_str(), (int)&doc);
    if ( abs_path[0] != '/' ) {
        printf("  not an absolute path\n");
        return;
    }
    val = find_node_from_path(&doc, abs_path, create);
    // pretty_print();
}

PropertyNode::PropertyNode(Value *v) {
    val = v;
}

bool PropertyNode::hasChild( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return true;
        }
    }
    return false;
}

PropertyNode PropertyNode::getChild( const char *name, bool create ) {
    if ( val->IsObject() ) {
        Value *child = find_node_from_path(val, name, create);
        return PropertyNode(child);
    }
    printf("%s not an object...\n", name);
    return PropertyNode();
}

bool PropertyNode::isNull() {
    return val == nullptr;
}

int PropertyNode::getLen( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            Value &v = (*val)[name];
            if ( v.IsArray() ) {
                return v.Size();
            }
        }
    }
    return 0;
}

vector<string> PropertyNode::getChildren(bool expand) {
    vector<string> result;
    if ( val->IsObject() ) {
        for (Value::ConstMemberIterator itr = val->MemberBegin(); itr != val->MemberEnd(); ++itr) {
            string name = itr->name.GetString();
            if ( expand and itr->value.IsArray() ) {
                for ( int i = 0; i < itr->value.Size(); i++ ) {
                    string ename = name + "/" + std::to_string(i);
                    result.push_back(ename);
                }
            } else {
                result.push_back(name);
            }
        }
    }
    return result;
}

static bool getValueAsBool( Value &v ) {
    if ( v.IsBool() ) {
        return v.GetBool();
    } else if ( v.IsInt() ) {
        return v.GetInt();
    } else if ( v.IsUint() ) {
        return v.GetUint();
    } else if ( v.IsInt64() ) {
        return v.GetInt64();
    } else if ( v.IsUint64() ) {
        return v.GetUint64();
    } else if ( v.IsFloat() ) {
        return v.GetFloat() == 0.0;
    } else if ( v.IsDouble() ) {
        return v.GetDouble() == 0.0;
    } else if ( v.IsString() ) {
        string s = v.GetString();
        if ( s == "true" or s == "True" or s == "TRUE" ) {
            return true;
        } else {
            return false;
        }
    } else {
        printf("Unknown type in getValueAsBool()\n");
    }
    return false;
}

static int getValueAsInt( Value &v ) {
    if ( v.IsBool() ) {
        return v.GetBool();
    } else if ( v.IsInt() ) {
        return v.GetInt();
    } else if ( v.IsUint() ) {
        return v.GetUint();
    } else if ( v.IsInt64() ) {
        return v.GetInt64();
    } else if ( v.IsUint64() ) {
        return v.GetUint64();
    } else if ( v.IsFloat() ) {
        return v.GetFloat();
    } else if ( v.IsDouble() ) {
        return v.GetDouble();
    } else if ( v.IsString() ) {
        string s = v.GetString();
        return std::stoi(s);
    } else {
        printf("Unknown type in getValueAsInt()\n");
    }
    return 0;
}

static unsigned int getValueAsUInt( Value &v ) {
    if ( v.IsBool() ) {
        return v.GetBool();
    } else if ( v.IsInt() ) {
        return v.GetInt();
    } else if ( v.IsUint() ) {
        return v.GetUint();
    } else if ( v.IsInt64() ) {
        return v.GetInt64();
    } else if ( v.IsUint64() ) {
        return v.GetUint64();
    } else if ( v.IsFloat() ) {
        return v.GetFloat();
    } else if ( v.IsDouble() ) {
        return v.GetDouble();
    } else if ( v.IsString() ) {
        string s = v.GetString();
        return std::stoi(s);
    } else {
        printf("Unknown type in getValueAsUInt()\n");
    }
    return 0;
}

static float getValueAsFloat( Value &v ) {
    if ( v.IsBool() ) {
        return v.GetBool();
    } else if ( v.IsInt() ) {
        return v.GetInt();
    } else if ( v.IsUint() ) {
        return v.GetUint();
    } else if ( v.IsInt64() ) {
        return v.GetInt64();
    } else if ( v.IsUint64() ) {
        return v.GetUint64();
    } else if ( v.IsFloat() ) {
        return v.GetFloat();
    } else if ( v.IsDouble() ) {
        return v.GetDouble();
    } else if ( v.IsString() ) {
        string s = v.GetString();
        return std::stof(s);
    } else {
        printf("Unknown type in getValueAsFloat()\n");
    }
    return 0.0;
}

static double getValueAsDouble( Value &v ) {
    if ( v.IsBool() ) {
        return v.GetBool();
    } else if ( v.IsInt() ) {
        return v.GetInt();
    } else if ( v.IsUint() ) {
        return v.GetUint();
    } else if ( v.IsInt64() ) {
        return v.GetInt64();
    } else if ( v.IsUint64() ) {
        return v.GetUint64();
    } else if ( v.IsFloat() ) {
        return v.GetFloat();
    } else if ( v.IsDouble() ) {
        return v.GetDouble();
    } else if ( v.IsString() ) {
        string s = v.GetString();
        return std::stod(s);
    } else {
        printf("Unknown type in getValueAsDouble()\n");
    }
    return 0.0;
}

static string getValueAsString( Value &v ) {
    if ( v.IsBool() ) {
        if ( v.GetBool() ) {
            return "true";
        } else {
            return "false";
        }
    } else if ( v.IsInt() ) {
        return std::to_string(v.GetInt());
    } else if ( v.IsUint() ) {
        return std::to_string(v.GetUint());
    } else if ( v.IsInt64() ) {
        return std::to_string(v.GetInt64());
    } else if ( v.IsUint64() ) {
        return std::to_string(v.GetUint64());
    } else if ( v.IsFloat() ) {
#if defined(ARDUPILOT_BUILD)
        char buf[30];
        hal.util->snprintf(buf, 30, "%f", v.GetFloat());
        return buf;
#else
        return std::to_string(v.GetFloat());
#endif
    } else if ( v.IsDouble() ) {
#if defined(ARDUPILOT_BUILD)
        char buf[30];
        hal.util->snprintf(buf, 30, "%lf", v.GetDouble());
        return buf;
#else
        return std::to_string(v.GetDouble());
#endif
    } else if ( v.IsString() ) {
        return v.GetString();
    }
    printf("Unknown type in getValueAsString()\n");
    return "unhandled value type";
}

bool PropertyNode::getBool( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return getValueAsBool((*val)[name]);
        }
    }
    return false;
}

int PropertyNode::getInt( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return getValueAsInt((*val)[name]);
        }
    }
    return 0;
}

unsigned int PropertyNode::getUInt( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return getValueAsUInt((*val)[name]);
        }
    }
    return 0;
}

float PropertyNode::getFloat( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return getValueAsFloat((*val)[name]);
        // } else {
        //     printf("no member in getFloat(%s)\n", name);
        }
    } else {
        printf("v is not an object\n");
    }
    return 0.0;
}

double PropertyNode::getDouble( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return getValueAsDouble((*val)[name]);
        }
    }
    return 0.0;
}

string PropertyNode::getString( const char *name ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            return getValueAsString((*val)[name]);
        } else {
            return (string)name + ": not a member";
        }
    }
    return (string)name + ": not an object";
}

float PropertyNode::getFloat( const char *name, int index ) {
    if ( val->IsObject() ) {
        if ( val->HasMember(name) ) {
            Value &v = (*val)[name];
            if ( v.IsArray() ) {
                if ( index >= 0 and index < v.Size() ) {
                    return getValueAsFloat(v[index]);
                } else {
                    printf("index out of bounds: %s\n", name);
                }
            } else {
                printf("not an array: %s\n", name);
            }
        } else {
            printf("no member in getFloat(%s, %d)\n", name, index);
        }
    } else {
        printf("v is not an object\n");
    }
    return 0.0;
}

bool PropertyNode::setBool( const char *name, bool b ) {
    if ( !val->IsObject() ) {
        val->SetObject();
    }
    Value newval(b);
    if ( !val->HasMember(name) ) {
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        val->AddMember(key, newval, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
    }
    (*val)[name] = b;
    return true;
}

bool PropertyNode::setInt( const char *name, int n ) {
    if ( !val->IsObject() ) {
        val->SetObject();
    }
    Value newval(n);
    if ( !val->HasMember(name) ) {
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        val->AddMember(key, newval, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
    }
    (*val)[name] = n;
    return true;
}

bool PropertyNode::setUInt( const char *name, unsigned int u ) {
    if ( !val->IsObject() ) {
        val->SetObject();
    }
    Value newval(u);
    if ( !val->HasMember(name) ) {
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        val->AddMember(key, newval, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
    }
    (*val)[name] = u;
    return true;
}

bool PropertyNode::setFloat( const char *name, float x ) {
    //printf("setFloat(%s) = %f\n", name, val);
    // hal.scheduler->delay(100);
    if ( !val->IsObject() ) {
        printf("  converting value to object\n");
        // hal.scheduler->delay(100);
        val->SetObject();
    }
    // printf("  creating newval\n");
    // hal.scheduler->delay(100);
    Value newval(x);
    if ( !val->HasMember(name) ) {
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        val->AddMember(key, newval, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
    }
    (*val)[name] = x;
    // hal.scheduler->delay(100);
    return true;
}

bool PropertyNode::setDouble( const char *name, double x ) {
    if ( !val->IsObject() ) {
        val->SetObject();
    }
    Value newval(x);
    if ( !val->HasMember(name) ) {
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        val->AddMember(key, newval, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
    }
    (*val)[name] = x;
    return true;
}

bool PropertyNode::setString( const char *name, string s ) {
    if ( !val->IsObject() ) {
        val->SetObject();
    }
    if ( !val->HasMember(name) ) {
        Value newval("");
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        val->AddMember(key, newval, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
    }
    (*val)[name].SetString(s.c_str(), s.length());
    return true;
}

bool PropertyNode::setFloat( const char *name, int index, float x ) {
    if ( !val->IsObject() ) {
        printf("  converting value to object\n");
        // hal.scheduler->delay(100);
        val->SetObject();
    }
    if ( !val->HasMember(name) ) {
        printf("creating %s\n", name);
        Value key(name, doc.GetAllocator());
        Value a(kArrayType);
        val->AddMember(key, a, doc.GetAllocator());
    } else {
        // printf("%s already exists\n", name);
        Value &a = (*val)[name];
        if ( ! a.IsArray() ) {
            printf("converting member to array: %s\n", name);
            a.SetArray();
        }
    }
    Value &a = (*val)[name];
    extend_array(&a, index);    // protect against out of range
    a[index] = x;
    return true;
}

static bool load_json( const char *file_path, Value *v ) {
    char read_buf[4096];
    printf("reading from %s\n", file_path);
    
    // open a file in read mode
#if defined(ARDUPILOT_BUILD)
    const int open_fd = AP::FS().open(file_path, O_RDONLY);
#else
    const int open_fd = open(file_path, O_RDONLY);
#endif
    if (open_fd == -1) {
        printf("Open %s failed\n", file_path);
        return false;
    }

    // read from file
    ssize_t read_size;
#if defined(ARDUINO_BUILD)
    read_size = AP::FS().read(open_fd, read_buf, sizeof(read_buf));
#else
    read_size = read(open_fd, read_buf, sizeof(read_buf));
#endif
    if ( read_size == -1 ) {
        printf("Read failed - %s\n", strerror(errno));
        return false;
    }

    // close file after reading
#if defined(ARDUINO_BUILD)
    AP::FS().close(open_fd);
#else
    close(open_fd);
#endif

    if ( read_size >= 0 ) {
        read_buf[read_size] = 0; // null terminate
    }
    printf("Read %d bytes.\n", read_size);
    // printf("Read %d bytes.\nstring: %s\n", read_size, read_buf);
    // hal.scheduler->delay(100);

    Document tmpdoc(&doc.GetAllocator());
    tmpdoc.Parse(read_buf);
    if ( tmpdoc.HasParseError() ){
        printf("json parse err: %d (%s)\n",
               tmpdoc.GetParseError(),
               GetParseError_En(tmpdoc.GetParseError()));
        return false;
    }

    // merge each new top level member individually
    for (Value::ConstMemberIterator itr = tmpdoc.MemberBegin(); itr != tmpdoc.MemberEnd(); ++itr) {
        printf(" merging: %s\n", itr->name.GetString());
        Value key;
        key.SetString(itr->name.GetString(), itr->name.GetStringLength(), doc.GetAllocator());
        Value &newval = tmpdoc[itr->name.GetString()];
        v->AddMember(key, newval, doc.GetAllocator());
    }

    return true;
}

// fixme: currently no mechanism to override include values
static void recursively_expand_includes(Value *v) {
    if ( v->IsObject() ) {
        if ( v->HasMember("include") and (*v)["include"].IsString() ) {
            printf("Need to include: %s\n", (*v)["include"].GetString());
            load_json( (*v)["include"].GetString(), v );
            v->RemoveMember("include");
        } else {
            for (Value::MemberIterator itr = v->MemberBegin(); itr != v->MemberEnd(); ++itr) {
                if ( itr->value.IsObject() ) {
                    recursively_expand_includes( &itr->value );
                }
            }
        }
    }
}

bool PropertyNode::load( const char *file_path ) {
    if ( !load_json(file_path, val) ) {
        return false;
    }
    recursively_expand_includes(val);
    
    printf("Updated node contents:\n");
    pretty_print();
    printf("\n");

    return true;
}

// void PropertyNode::print() {
//     StringBuffer buffer;
//     Writer<StringBuffer> writer(buffer);
//     val->Accept(writer);
//     //const char* output = buffer.GetString();
//     printf("%s\n", buffer.GetString());
// }

void PropertyNode::pretty_print() {
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    val->Accept(writer);
#if defined(ARDUPILOT_BUILD)
    // work around size limitations
    const char *ptr = buffer.GetString();
    for ( int i = 0; i < buffer.GetSize(); i++ ) {
        printf("%c", ptr[i]);
        if ( i % 256 == 0 ) {
            hal.scheduler->delay(100);
        }
    }
#else
    printf("%s", buffer.GetString());
#endif
}

Document doc;

#if 0
int main() {
   // suck in all the input
    string input_buf = "";
    while ( true ) {
        char c = getchar();
        if ( c == EOF ) {
            break;
        }
        input_buf += c;
    }

    // doc.Parse(input_buf.c_str());

    PropertyNode n1 = PropertyNode("/a/b/c/d", true);
    PropertyNode n2 = PropertyNode("/a/b/c/d", true);
    n1.setInt("curt", 53);
    printf("%ld\n", n1.getInt("curt"));
    n1.setInt("curt", 55);
    printf("%ld\n", n1.getInt("curt"));
    printf("As bool: %d\n", n1.getBool("curt"));
    printf("As double: %.2f\n", n1.getDouble("curt"));
    string s = n1.getString("curt");
    printf("As string: %s\n", s.c_str());
    n1.setString("foo", "1.2345");
    printf("As double: %.2f\n", n1.getDouble("foo"));
    printf("As int: %d\n", n1.getInt("foo"));
    PropertyNode("/").pretty_print();
}
#endif
