// precompiled header
#include <includes.hpp>


// Generated from .\grammar\gsc.g4 by ANTLR 4.13.0


#include "gscVisitor.h"

#include "gscParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct GscParserStaticData final {
  GscParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  GscParserStaticData(const GscParserStaticData&) = delete;
  GscParserStaticData(GscParserStaticData&&) = delete;
  GscParserStaticData& operator=(const GscParserStaticData&) = delete;
  GscParserStaticData& operator=(GscParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag gscParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
GscParserStaticData *gscParserStaticData = nullptr;

void gscParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (gscParserStaticData != nullptr) {
    return;
  }
#else
  assert(gscParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<GscParserStaticData>(
    std::vector<std::string>{
      "prog", "include", "using_animtree", "precache", "namespace", "filenamespace", 
      "constexpr", "class_def", "class_var", "function", "detour_info", 
      "param_list", "param_val", "expression_list", "statement_block", "statement", 
      "statement_dev_block", "statement_for", "statement_while", "statement_dowhile", 
      "statement_foreach", "statement_if", "statement_switch", "statement_inst", 
      "nop_def", "devop_def", "function_component", "operator_inst", "array_unpack", 
      "expression", "set_expression", "expression0", "expression1", "expression2", 
      "expression3", "expression4", "expression5", "expression6", "expression7", 
      "expression8", "expression9", "expression10", "expression11", "expression12", 
      "expression13", "expression14", "expression15", "is_expression", "function_call_exp", 
      "function_call", "left_value", "const_expr", "const_expr_static", 
      "function_ref", "anim_ref", "data_ref", "number", "vector_value", 
      "array_def", "struct_def", "class_init", "idf"
    },
    std::vector<std::string>{
      "", "'/#'", "'#/'", "'#include'", "'#using'", "';'", "'#using_animtree'", 
      "'('", "')'", "'#precache'", "','", "'#namespace'", "'#file'", "'#constexpr'", 
      "'#define'", "'='", "'class'", "':'", "'{'", "'}'", "'var'", "'function'", 
      "'private'", "'autoexec'", "'event_handler'", "'['", "']'", "'=>'", 
      "'detour'", "'<'", "'>'", "'::'", "'*'", "'&'", "'...'", "'for'", 
      "'while'", "'do'", "'foreach'", "'in'", "'if'", "'else'", "'switch'", 
      "'case'", "'default'", "'nop'", "'Nop'", "'DevOp'", "'devop'", "'Devop'", 
      "'->'", "'\\u003F'", "'+='", "'-='", "'/='", "'*='", "'%='", "'&='", 
      "'|='", "'^='", "'<<='", "'>>='", "'~='", "'\\u003F\\u003F'", "'||'", 
      "'&&'", "'|'", "'^'", "'!='", "'=='", "'!=='", "'==='", "'<='", "'>='", 
      "'<=='", "'>=='", "'<<'", "'>>'", "'+'", "'-'", "'/'", "'%'", "'!'", 
      "'~'", "'++'", "'--'", "'is'", "'not'", "'thread'", "'childthread'", 
      "'threadendon'", "'builtin'", "'.'", "'\\u003F.'", "'@'", "'new'", 
      "", "", "", "", "", "", "", "", "", "'undefined'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "NEWLINE", "WHITESPACE", 
      "INTEGER10", "INTEGER16", "INTEGER8", "INTEGER2", "FLOATVAL", "BUILTIN", 
      "BOOL_VALUE", "UNDEFINED_VALUE", "IDENTIFIER", "SCR_HASH", "ANIMTREE_IDENTIFIER", 
      "PATH", "STRING", "ISTRING", "HASHSTRING"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,112,849,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
  	21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
  	28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
  	35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
  	42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
  	49,2,50,7,50,2,51,7,51,2,52,7,52,2,53,7,53,2,54,7,54,2,55,7,55,2,56,7,
  	56,2,57,7,57,2,58,7,58,2,59,7,59,2,60,7,60,2,61,7,61,1,0,1,0,1,0,1,0,
  	1,0,1,0,1,0,1,0,1,0,1,0,5,0,135,8,0,10,0,12,0,138,9,0,1,0,1,0,1,1,1,1,
  	1,1,1,1,1,2,1,2,1,2,1,2,1,2,1,2,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,4,1,
  	4,1,4,1,4,1,5,1,5,1,5,1,5,1,6,1,6,1,6,1,6,1,6,1,6,1,7,1,7,1,7,1,7,1,7,
  	1,7,5,7,180,8,7,10,7,12,7,183,9,7,3,7,185,8,7,1,7,1,7,5,7,189,8,7,10,
  	7,12,7,192,9,7,1,7,5,7,195,8,7,10,7,12,7,198,9,7,1,7,1,7,1,8,1,8,1,8,
  	1,8,3,8,206,8,8,1,8,1,8,1,9,3,9,211,8,9,1,9,3,9,214,8,9,1,9,1,9,1,9,1,
  	9,1,9,3,9,221,8,9,3,9,223,8,9,1,9,1,9,1,9,1,9,3,9,229,8,9,1,9,3,9,232,
  	8,9,1,9,3,9,235,8,9,1,9,1,9,1,9,1,9,3,9,241,8,9,1,9,1,9,1,10,1,10,1,10,
  	1,10,1,10,1,10,1,10,3,10,252,8,10,1,11,1,11,1,11,5,11,257,8,11,10,11,
  	12,11,260,9,11,3,11,262,8,11,1,12,3,12,265,8,12,1,12,1,12,1,12,3,12,270,
  	8,12,1,12,3,12,273,8,12,1,13,1,13,1,13,5,13,278,8,13,10,13,12,13,281,
  	9,13,3,13,283,8,13,1,14,1,14,5,14,287,8,14,10,14,12,14,290,9,14,1,14,
  	1,14,1,15,1,15,3,15,296,8,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,
  	3,15,306,8,15,1,16,1,16,5,16,310,8,16,10,16,12,16,313,9,16,1,16,1,16,
  	1,17,1,17,1,17,3,17,320,8,17,1,17,1,17,3,17,324,8,17,1,17,1,17,3,17,328,
  	8,17,1,17,1,17,1,17,1,18,1,18,1,18,1,18,1,18,1,18,1,19,1,19,1,19,1,19,
  	1,19,1,19,1,19,1,20,1,20,1,20,1,20,1,20,3,20,351,8,20,1,20,1,20,1,20,
  	1,20,1,20,1,21,1,21,1,21,1,21,1,21,1,21,1,21,3,21,365,8,21,1,22,1,22,
  	1,22,1,22,1,22,1,22,1,22,1,22,3,22,375,8,22,1,22,1,22,5,22,379,8,22,10,
  	22,12,22,382,9,22,4,22,384,8,22,11,22,12,22,385,1,22,1,22,1,23,1,23,1,
  	23,1,23,1,23,1,23,1,23,3,23,397,8,23,1,23,1,23,1,24,1,24,1,24,1,24,1,
  	24,3,24,406,8,24,1,25,1,25,1,25,1,25,1,25,1,26,1,26,3,26,415,8,26,1,26,
  	1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,
  	3,26,432,8,26,1,27,1,27,1,27,3,27,437,8,27,1,28,1,28,1,28,1,28,5,28,443,
  	8,28,10,28,12,28,446,9,28,1,28,1,28,1,28,1,28,1,29,1,29,1,29,1,29,1,29,
  	1,29,1,29,1,29,3,29,460,8,29,1,30,1,30,1,30,1,30,1,31,1,31,1,31,1,31,
  	1,31,1,31,5,31,472,8,31,10,31,12,31,475,9,31,1,32,1,32,1,32,1,32,1,32,
  	1,32,5,32,483,8,32,10,32,12,32,486,9,32,1,33,1,33,1,33,1,33,1,33,1,33,
  	5,33,494,8,33,10,33,12,33,497,9,33,1,34,1,34,1,34,1,34,1,34,1,34,5,34,
  	505,8,34,10,34,12,34,508,9,34,1,35,1,35,1,35,1,35,1,35,1,35,5,35,516,
  	8,35,10,35,12,35,519,9,35,1,36,1,36,1,36,1,36,1,36,1,36,5,36,527,8,36,
  	10,36,12,36,530,9,36,1,37,1,37,1,37,1,37,1,37,1,37,5,37,538,8,37,10,37,
  	12,37,541,9,37,1,38,1,38,1,38,1,38,1,38,1,38,5,38,549,8,38,10,38,12,38,
  	552,9,38,1,39,1,39,1,39,1,39,1,39,1,39,5,39,560,8,39,10,39,12,39,563,
  	9,39,1,40,1,40,1,40,1,40,1,40,1,40,5,40,571,8,40,10,40,12,40,574,9,40,
  	1,41,1,41,1,41,1,41,1,41,1,41,5,41,582,8,41,10,41,12,41,585,9,41,1,42,
  	1,42,1,42,1,42,1,42,1,42,5,42,593,8,42,10,42,12,42,596,9,42,1,43,1,43,
  	1,43,1,43,1,43,1,43,1,43,1,43,1,43,3,43,607,8,43,1,44,1,44,3,44,611,8,
  	44,1,45,1,45,1,45,3,45,616,8,45,1,46,1,46,1,46,1,46,1,47,1,47,1,47,3,
  	47,625,8,47,1,47,1,47,1,48,1,48,1,48,3,48,632,8,48,1,48,1,48,1,48,1,48,
  	1,48,3,48,639,8,48,1,49,1,49,3,49,643,8,49,1,49,1,49,1,49,1,49,1,49,1,
  	49,1,49,3,49,652,8,49,1,49,3,49,655,8,49,1,49,1,49,1,49,1,49,1,49,3,49,
  	662,8,49,1,49,1,49,3,49,666,8,49,1,49,1,49,1,49,1,49,1,49,5,49,673,8,
  	49,10,49,12,49,676,9,49,1,50,1,50,1,50,1,50,1,50,3,50,683,8,50,1,50,1,
  	50,1,50,1,50,1,50,1,50,3,50,691,8,50,1,50,1,50,1,50,3,50,696,8,50,1,50,
  	1,50,1,50,1,50,3,50,702,8,50,1,50,1,50,1,50,1,50,1,50,1,50,1,50,3,50,
  	711,8,50,1,50,1,50,1,50,1,50,1,50,5,50,718,8,50,10,50,12,50,721,9,50,
  	1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,3,51,732,8,51,1,52,1,52,
  	1,52,1,52,1,52,1,52,1,52,1,52,1,52,3,52,743,8,52,1,53,1,53,1,53,3,53,
  	748,8,53,1,53,1,53,1,53,1,53,1,53,1,53,1,53,1,53,1,53,1,53,3,53,760,8,
  	53,1,54,1,54,1,54,1,54,3,54,766,8,54,1,55,1,55,1,55,1,55,1,55,1,55,1,
  	55,1,56,1,56,1,57,1,57,1,57,1,57,1,57,1,57,1,57,1,57,1,58,1,58,1,58,1,
  	58,3,58,789,8,58,1,58,1,58,1,58,1,58,1,58,3,58,796,8,58,1,58,5,58,799,
  	8,58,10,58,12,58,802,9,58,1,58,3,58,805,8,58,3,58,807,8,58,1,58,1,58,
  	1,59,1,59,1,59,1,59,1,59,1,59,1,59,1,59,1,59,5,59,820,8,59,10,59,12,59,
  	823,9,59,1,59,3,59,826,8,59,3,59,828,8,59,1,59,1,59,1,60,1,60,1,60,1,
  	60,1,60,1,60,5,60,838,8,60,10,60,12,60,841,9,60,3,60,843,8,60,1,60,1,
  	60,1,61,1,61,1,61,0,14,62,64,66,68,70,72,74,76,78,80,82,84,98,100,62,
  	0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,
  	50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,
  	96,98,100,102,104,106,108,110,112,114,116,118,120,122,0,19,1,0,3,4,2,
  	0,106,106,109,109,1,0,13,14,1,0,32,33,1,0,45,46,1,0,47,49,2,0,15,15,52,
  	62,1,0,68,71,2,0,29,30,72,75,2,0,29,29,72,72,1,0,76,77,1,0,78,79,2,0,
  	32,32,80,81,1,0,82,83,1,0,84,85,2,0,21,21,104,106,1,0,88,91,1,0,92,93,
  	2,0,98,101,106,106,918,0,136,1,0,0,0,2,141,1,0,0,0,4,145,1,0,0,0,6,151,
  	1,0,0,0,8,159,1,0,0,0,10,163,1,0,0,0,12,167,1,0,0,0,14,173,1,0,0,0,16,
  	201,1,0,0,0,18,210,1,0,0,0,20,244,1,0,0,0,22,261,1,0,0,0,24,272,1,0,0,
  	0,26,282,1,0,0,0,28,284,1,0,0,0,30,295,1,0,0,0,32,307,1,0,0,0,34,316,
  	1,0,0,0,36,332,1,0,0,0,38,338,1,0,0,0,40,345,1,0,0,0,42,357,1,0,0,0,44,
  	366,1,0,0,0,46,396,1,0,0,0,48,400,1,0,0,0,50,407,1,0,0,0,52,431,1,0,0,
  	0,54,433,1,0,0,0,56,438,1,0,0,0,58,459,1,0,0,0,60,461,1,0,0,0,62,465,
  	1,0,0,0,64,476,1,0,0,0,66,487,1,0,0,0,68,498,1,0,0,0,70,509,1,0,0,0,72,
  	520,1,0,0,0,74,531,1,0,0,0,76,542,1,0,0,0,78,553,1,0,0,0,80,564,1,0,0,
  	0,82,575,1,0,0,0,84,586,1,0,0,0,86,606,1,0,0,0,88,610,1,0,0,0,90,615,
  	1,0,0,0,92,617,1,0,0,0,94,621,1,0,0,0,96,638,1,0,0,0,98,661,1,0,0,0,100,
  	701,1,0,0,0,102,731,1,0,0,0,104,742,1,0,0,0,106,759,1,0,0,0,108,761,1,
  	0,0,0,110,767,1,0,0,0,112,774,1,0,0,0,114,776,1,0,0,0,116,784,1,0,0,0,
  	118,810,1,0,0,0,120,831,1,0,0,0,122,846,1,0,0,0,124,135,5,1,0,0,125,135,
  	5,2,0,0,126,135,3,18,9,0,127,135,3,2,1,0,128,135,3,6,3,0,129,135,3,4,
  	2,0,130,135,3,8,4,0,131,135,3,10,5,0,132,135,3,12,6,0,133,135,3,14,7,
  	0,134,124,1,0,0,0,134,125,1,0,0,0,134,126,1,0,0,0,134,127,1,0,0,0,134,
  	128,1,0,0,0,134,129,1,0,0,0,134,130,1,0,0,0,134,131,1,0,0,0,134,132,1,
  	0,0,0,134,133,1,0,0,0,135,138,1,0,0,0,136,134,1,0,0,0,136,137,1,0,0,0,
  	137,139,1,0,0,0,138,136,1,0,0,0,139,140,5,0,0,1,140,1,1,0,0,0,141,142,
  	7,0,0,0,142,143,7,1,0,0,143,144,5,5,0,0,144,3,1,0,0,0,145,146,5,6,0,0,
  	146,147,5,7,0,0,147,148,5,110,0,0,148,149,5,8,0,0,149,150,5,5,0,0,150,
  	5,1,0,0,0,151,152,5,9,0,0,152,153,5,7,0,0,153,154,5,110,0,0,154,155,5,
  	10,0,0,155,156,5,110,0,0,156,157,5,8,0,0,157,158,5,5,0,0,158,7,1,0,0,
  	0,159,160,5,11,0,0,160,161,5,106,0,0,161,162,5,5,0,0,162,9,1,0,0,0,163,
  	164,5,12,0,0,164,165,7,1,0,0,165,166,5,5,0,0,166,11,1,0,0,0,167,168,7,
  	2,0,0,168,169,5,106,0,0,169,170,5,15,0,0,170,171,3,58,29,0,171,172,5,
  	5,0,0,172,13,1,0,0,0,173,174,5,16,0,0,174,184,5,106,0,0,175,176,5,17,
  	0,0,176,181,5,106,0,0,177,178,5,10,0,0,178,180,5,106,0,0,179,177,1,0,
  	0,0,180,183,1,0,0,0,181,179,1,0,0,0,181,182,1,0,0,0,182,185,1,0,0,0,183,
  	181,1,0,0,0,184,175,1,0,0,0,184,185,1,0,0,0,185,186,1,0,0,0,186,190,5,
  	18,0,0,187,189,3,16,8,0,188,187,1,0,0,0,189,192,1,0,0,0,190,188,1,0,0,
  	0,190,191,1,0,0,0,191,196,1,0,0,0,192,190,1,0,0,0,193,195,3,18,9,0,194,
  	193,1,0,0,0,195,198,1,0,0,0,196,194,1,0,0,0,196,197,1,0,0,0,197,199,1,
  	0,0,0,198,196,1,0,0,0,199,200,5,19,0,0,200,15,1,0,0,0,201,202,5,20,0,
  	0,202,205,5,106,0,0,203,204,5,15,0,0,204,206,3,58,29,0,205,203,1,0,0,
  	0,205,206,1,0,0,0,206,207,1,0,0,0,207,208,5,5,0,0,208,17,1,0,0,0,209,
  	211,5,21,0,0,210,209,1,0,0,0,210,211,1,0,0,0,211,213,1,0,0,0,212,214,
  	5,22,0,0,213,212,1,0,0,0,213,214,1,0,0,0,214,222,1,0,0,0,215,220,5,23,
  	0,0,216,217,5,7,0,0,217,218,3,112,56,0,218,219,5,8,0,0,219,221,1,0,0,
  	0,220,216,1,0,0,0,220,221,1,0,0,0,221,223,1,0,0,0,222,215,1,0,0,0,222,
  	223,1,0,0,0,223,228,1,0,0,0,224,225,5,24,0,0,225,226,5,25,0,0,226,227,
  	5,106,0,0,227,229,5,26,0,0,228,224,1,0,0,0,228,229,1,0,0,0,229,231,1,
  	0,0,0,230,232,3,20,10,0,231,230,1,0,0,0,231,232,1,0,0,0,232,234,1,0,0,
  	0,233,235,5,106,0,0,234,233,1,0,0,0,234,235,1,0,0,0,235,236,1,0,0,0,236,
  	237,5,7,0,0,237,238,3,22,11,0,238,240,5,8,0,0,239,241,5,27,0,0,240,239,
  	1,0,0,0,240,241,1,0,0,0,241,242,1,0,0,0,242,243,3,28,14,0,243,19,1,0,
  	0,0,244,245,5,28,0,0,245,251,5,106,0,0,246,247,5,29,0,0,247,248,5,109,
  	0,0,248,249,5,30,0,0,249,250,5,31,0,0,250,252,5,106,0,0,251,246,1,0,0,
  	0,251,252,1,0,0,0,252,21,1,0,0,0,253,258,3,24,12,0,254,255,5,10,0,0,255,
  	257,3,24,12,0,256,254,1,0,0,0,257,260,1,0,0,0,258,256,1,0,0,0,258,259,
  	1,0,0,0,259,262,1,0,0,0,260,258,1,0,0,0,261,253,1,0,0,0,261,262,1,0,0,
  	0,262,23,1,0,0,0,263,265,7,3,0,0,264,263,1,0,0,0,264,265,1,0,0,0,265,
  	266,1,0,0,0,266,269,5,106,0,0,267,268,5,15,0,0,268,270,3,58,29,0,269,
  	267,1,0,0,0,269,270,1,0,0,0,270,273,1,0,0,0,271,273,5,34,0,0,272,264,
  	1,0,0,0,272,271,1,0,0,0,273,25,1,0,0,0,274,279,3,58,29,0,275,276,5,10,
  	0,0,276,278,3,58,29,0,277,275,1,0,0,0,278,281,1,0,0,0,279,277,1,0,0,0,
  	279,280,1,0,0,0,280,283,1,0,0,0,281,279,1,0,0,0,282,274,1,0,0,0,282,283,
  	1,0,0,0,283,27,1,0,0,0,284,288,5,18,0,0,285,287,3,30,15,0,286,285,1,0,
  	0,0,287,290,1,0,0,0,288,286,1,0,0,0,288,289,1,0,0,0,289,291,1,0,0,0,290,
  	288,1,0,0,0,291,292,5,19,0,0,292,29,1,0,0,0,293,294,5,106,0,0,294,296,
  	5,17,0,0,295,293,1,0,0,0,295,296,1,0,0,0,296,305,1,0,0,0,297,306,3,28,
  	14,0,298,306,3,32,16,0,299,306,3,34,17,0,300,306,3,42,21,0,301,306,3,
  	36,18,0,302,306,3,40,20,0,303,306,3,46,23,0,304,306,3,44,22,0,305,297,
  	1,0,0,0,305,298,1,0,0,0,305,299,1,0,0,0,305,300,1,0,0,0,305,301,1,0,0,
  	0,305,302,1,0,0,0,305,303,1,0,0,0,305,304,1,0,0,0,306,31,1,0,0,0,307,
  	311,5,1,0,0,308,310,3,30,15,0,309,308,1,0,0,0,310,313,1,0,0,0,311,309,
  	1,0,0,0,311,312,1,0,0,0,312,314,1,0,0,0,313,311,1,0,0,0,314,315,5,2,0,
  	0,315,33,1,0,0,0,316,317,5,35,0,0,317,319,5,7,0,0,318,320,3,58,29,0,319,
  	318,1,0,0,0,319,320,1,0,0,0,320,321,1,0,0,0,321,323,5,5,0,0,322,324,3,
  	58,29,0,323,322,1,0,0,0,323,324,1,0,0,0,324,325,1,0,0,0,325,327,5,5,0,
  	0,326,328,3,58,29,0,327,326,1,0,0,0,327,328,1,0,0,0,328,329,1,0,0,0,329,
  	330,5,8,0,0,330,331,3,30,15,0,331,35,1,0,0,0,332,333,5,36,0,0,333,334,
  	5,7,0,0,334,335,3,58,29,0,335,336,5,8,0,0,336,337,3,30,15,0,337,37,1,
  	0,0,0,338,339,5,37,0,0,339,340,3,30,15,0,340,341,5,36,0,0,341,342,5,7,
  	0,0,342,343,3,58,29,0,343,344,5,8,0,0,344,39,1,0,0,0,345,346,5,38,0,0,
  	346,347,5,7,0,0,347,350,5,106,0,0,348,349,5,10,0,0,349,351,5,106,0,0,
  	350,348,1,0,0,0,350,351,1,0,0,0,351,352,1,0,0,0,352,353,5,39,0,0,353,
  	354,3,58,29,0,354,355,5,8,0,0,355,356,3,30,15,0,356,41,1,0,0,0,357,358,
  	5,40,0,0,358,359,5,7,0,0,359,360,3,58,29,0,360,361,5,8,0,0,361,364,3,
  	30,15,0,362,363,5,41,0,0,363,365,3,30,15,0,364,362,1,0,0,0,364,365,1,
  	0,0,0,365,43,1,0,0,0,366,367,5,42,0,0,367,368,5,7,0,0,368,369,3,58,29,
  	0,369,370,5,8,0,0,370,383,5,18,0,0,371,372,5,43,0,0,372,375,3,102,51,
  	0,373,375,5,44,0,0,374,371,1,0,0,0,374,373,1,0,0,0,375,376,1,0,0,0,376,
  	380,5,17,0,0,377,379,3,30,15,0,378,377,1,0,0,0,379,382,1,0,0,0,380,378,
  	1,0,0,0,380,381,1,0,0,0,381,384,1,0,0,0,382,380,1,0,0,0,383,374,1,0,0,
  	0,384,385,1,0,0,0,385,383,1,0,0,0,385,386,1,0,0,0,386,387,1,0,0,0,387,
  	388,5,19,0,0,388,45,1,0,0,0,389,397,3,58,29,0,390,397,3,54,27,0,391,397,
  	3,38,19,0,392,397,3,96,48,0,393,397,3,48,24,0,394,397,3,50,25,0,395,397,
  	3,56,28,0,396,389,1,0,0,0,396,390,1,0,0,0,396,391,1,0,0,0,396,392,1,0,
  	0,0,396,393,1,0,0,0,396,394,1,0,0,0,396,395,1,0,0,0,397,398,1,0,0,0,398,
  	399,5,5,0,0,399,47,1,0,0,0,400,405,7,4,0,0,401,402,5,7,0,0,402,403,3,
  	112,56,0,403,404,5,8,0,0,404,406,1,0,0,0,405,401,1,0,0,0,405,406,1,0,
  	0,0,406,49,1,0,0,0,407,408,7,5,0,0,408,409,5,7,0,0,409,410,3,112,56,0,
  	410,411,5,8,0,0,411,51,1,0,0,0,412,413,5,106,0,0,413,415,5,31,0,0,414,
  	412,1,0,0,0,414,415,1,0,0,0,415,416,1,0,0,0,416,432,5,106,0,0,417,418,
  	5,25,0,0,418,419,5,25,0,0,419,420,3,58,29,0,420,421,5,26,0,0,421,422,
  	5,26,0,0,422,432,1,0,0,0,423,424,5,25,0,0,424,425,5,25,0,0,425,426,3,
  	58,29,0,426,427,5,26,0,0,427,428,5,26,0,0,428,429,5,50,0,0,429,430,5,
  	106,0,0,430,432,1,0,0,0,431,414,1,0,0,0,431,417,1,0,0,0,431,423,1,0,0,
  	0,432,53,1,0,0,0,433,436,5,103,0,0,434,437,5,106,0,0,435,437,3,58,29,
  	0,436,434,1,0,0,0,436,435,1,0,0,0,436,437,1,0,0,0,437,55,1,0,0,0,438,
  	439,5,25,0,0,439,444,5,106,0,0,440,441,5,10,0,0,441,443,5,106,0,0,442,
  	440,1,0,0,0,443,446,1,0,0,0,444,442,1,0,0,0,444,445,1,0,0,0,445,447,1,
  	0,0,0,446,444,1,0,0,0,447,448,5,26,0,0,448,449,5,15,0,0,449,450,3,58,
  	29,0,450,57,1,0,0,0,451,460,3,60,30,0,452,453,3,62,31,0,453,454,5,51,
  	0,0,454,455,3,58,29,0,455,456,5,17,0,0,456,457,3,58,29,0,457,460,1,0,
  	0,0,458,460,3,62,31,0,459,451,1,0,0,0,459,452,1,0,0,0,459,458,1,0,0,0,
  	460,59,1,0,0,0,461,462,3,100,50,0,462,463,7,6,0,0,463,464,3,58,29,0,464,
  	61,1,0,0,0,465,466,6,31,-1,0,466,467,3,64,32,0,467,473,1,0,0,0,468,469,
  	10,2,0,0,469,470,5,63,0,0,470,472,3,64,32,0,471,468,1,0,0,0,472,475,1,
  	0,0,0,473,471,1,0,0,0,473,474,1,0,0,0,474,63,1,0,0,0,475,473,1,0,0,0,
  	476,477,6,32,-1,0,477,478,3,66,33,0,478,484,1,0,0,0,479,480,10,2,0,0,
  	480,481,5,64,0,0,481,483,3,66,33,0,482,479,1,0,0,0,483,486,1,0,0,0,484,
  	482,1,0,0,0,484,485,1,0,0,0,485,65,1,0,0,0,486,484,1,0,0,0,487,488,6,
  	33,-1,0,488,489,3,68,34,0,489,495,1,0,0,0,490,491,10,2,0,0,491,492,5,
  	65,0,0,492,494,3,68,34,0,493,490,1,0,0,0,494,497,1,0,0,0,495,493,1,0,
  	0,0,495,496,1,0,0,0,496,67,1,0,0,0,497,495,1,0,0,0,498,499,6,34,-1,0,
  	499,500,3,70,35,0,500,506,1,0,0,0,501,502,10,2,0,0,502,503,5,66,0,0,503,
  	505,3,70,35,0,504,501,1,0,0,0,505,508,1,0,0,0,506,504,1,0,0,0,506,507,
  	1,0,0,0,507,69,1,0,0,0,508,506,1,0,0,0,509,510,6,35,-1,0,510,511,3,72,
  	36,0,511,517,1,0,0,0,512,513,10,2,0,0,513,514,5,67,0,0,514,516,3,72,36,
  	0,515,512,1,0,0,0,516,519,1,0,0,0,517,515,1,0,0,0,517,518,1,0,0,0,518,
  	71,1,0,0,0,519,517,1,0,0,0,520,521,6,36,-1,0,521,522,3,74,37,0,522,528,
  	1,0,0,0,523,524,10,2,0,0,524,525,5,33,0,0,525,527,3,74,37,0,526,523,1,
  	0,0,0,527,530,1,0,0,0,528,526,1,0,0,0,528,529,1,0,0,0,529,73,1,0,0,0,
  	530,528,1,0,0,0,531,532,6,37,-1,0,532,533,3,76,38,0,533,539,1,0,0,0,534,
  	535,10,2,0,0,535,536,7,7,0,0,536,538,3,76,38,0,537,534,1,0,0,0,538,541,
  	1,0,0,0,539,537,1,0,0,0,539,540,1,0,0,0,540,75,1,0,0,0,541,539,1,0,0,
  	0,542,543,6,38,-1,0,543,544,3,78,39,0,544,550,1,0,0,0,545,546,10,2,0,
  	0,546,547,7,8,0,0,547,549,3,78,39,0,548,545,1,0,0,0,549,552,1,0,0,0,550,
  	548,1,0,0,0,550,551,1,0,0,0,551,77,1,0,0,0,552,550,1,0,0,0,553,554,6,
  	39,-1,0,554,555,3,80,40,0,555,561,1,0,0,0,556,557,10,2,0,0,557,558,7,
  	9,0,0,558,560,3,80,40,0,559,556,1,0,0,0,560,563,1,0,0,0,561,559,1,0,0,
  	0,561,562,1,0,0,0,562,79,1,0,0,0,563,561,1,0,0,0,564,565,6,40,-1,0,565,
  	566,3,82,41,0,566,572,1,0,0,0,567,568,10,2,0,0,568,569,7,10,0,0,569,571,
  	3,82,41,0,570,567,1,0,0,0,571,574,1,0,0,0,572,570,1,0,0,0,572,573,1,0,
  	0,0,573,81,1,0,0,0,574,572,1,0,0,0,575,576,6,41,-1,0,576,577,3,84,42,
  	0,577,583,1,0,0,0,578,579,10,2,0,0,579,580,7,11,0,0,580,582,3,84,42,0,
  	581,578,1,0,0,0,582,585,1,0,0,0,583,581,1,0,0,0,583,584,1,0,0,0,584,83,
  	1,0,0,0,585,583,1,0,0,0,586,587,6,42,-1,0,587,588,3,86,43,0,588,594,1,
  	0,0,0,589,590,10,2,0,0,590,591,7,12,0,0,591,593,3,86,43,0,592,589,1,0,
  	0,0,593,596,1,0,0,0,594,592,1,0,0,0,594,595,1,0,0,0,595,85,1,0,0,0,596,
  	594,1,0,0,0,597,598,7,13,0,0,598,607,3,88,44,0,599,600,7,14,0,0,600,607,
  	3,100,50,0,601,602,3,100,50,0,602,603,7,14,0,0,603,607,1,0,0,0,604,607,
  	3,94,47,0,605,607,3,88,44,0,606,597,1,0,0,0,606,599,1,0,0,0,606,601,1,
  	0,0,0,606,604,1,0,0,0,606,605,1,0,0,0,607,87,1,0,0,0,608,611,3,96,48,
  	0,609,611,3,90,45,0,610,608,1,0,0,0,610,609,1,0,0,0,611,89,1,0,0,0,612,
  	616,3,102,51,0,613,616,3,92,46,0,614,616,3,100,50,0,615,612,1,0,0,0,615,
  	613,1,0,0,0,615,614,1,0,0,0,616,91,1,0,0,0,617,618,5,7,0,0,618,619,3,
  	58,29,0,619,620,5,8,0,0,620,93,1,0,0,0,621,622,3,88,44,0,622,624,5,86,
  	0,0,623,625,5,87,0,0,624,623,1,0,0,0,624,625,1,0,0,0,625,626,1,0,0,0,
  	626,627,7,15,0,0,627,95,1,0,0,0,628,639,3,98,49,0,629,631,3,90,45,0,630,
  	632,7,16,0,0,631,630,1,0,0,0,631,632,1,0,0,0,632,633,1,0,0,0,633,634,
  	3,52,26,0,634,635,5,7,0,0,635,636,3,26,13,0,636,637,5,8,0,0,637,639,1,
  	0,0,0,638,628,1,0,0,0,638,629,1,0,0,0,639,97,1,0,0,0,640,642,6,49,-1,
  	0,641,643,7,16,0,0,642,641,1,0,0,0,642,643,1,0,0,0,643,644,1,0,0,0,644,
  	645,3,52,26,0,645,646,5,7,0,0,646,647,3,26,13,0,647,648,5,8,0,0,648,662,
  	1,0,0,0,649,652,3,102,51,0,650,652,3,92,46,0,651,649,1,0,0,0,651,650,
  	1,0,0,0,652,654,1,0,0,0,653,655,7,16,0,0,654,653,1,0,0,0,654,655,1,0,
  	0,0,655,656,1,0,0,0,656,657,3,52,26,0,657,658,5,7,0,0,658,659,3,26,13,
  	0,659,660,5,8,0,0,660,662,1,0,0,0,661,640,1,0,0,0,661,651,1,0,0,0,662,
  	674,1,0,0,0,663,665,10,1,0,0,664,666,7,16,0,0,665,664,1,0,0,0,665,666,
  	1,0,0,0,666,667,1,0,0,0,667,668,3,52,26,0,668,669,5,7,0,0,669,670,3,26,
  	13,0,670,671,5,8,0,0,671,673,1,0,0,0,672,663,1,0,0,0,673,676,1,0,0,0,
  	674,672,1,0,0,0,674,675,1,0,0,0,675,99,1,0,0,0,676,674,1,0,0,0,677,678,
  	6,50,-1,0,678,702,3,122,61,0,679,683,3,98,49,0,680,683,3,102,51,0,681,
  	683,3,92,46,0,682,679,1,0,0,0,682,680,1,0,0,0,682,681,1,0,0,0,683,684,
  	1,0,0,0,684,690,7,17,0,0,685,691,3,122,61,0,686,687,5,7,0,0,687,688,3,
  	58,29,0,688,689,5,8,0,0,689,691,1,0,0,0,690,685,1,0,0,0,690,686,1,0,0,
  	0,691,702,1,0,0,0,692,696,3,98,49,0,693,696,3,102,51,0,694,696,3,92,46,
  	0,695,692,1,0,0,0,695,693,1,0,0,0,695,694,1,0,0,0,696,697,1,0,0,0,697,
  	698,5,25,0,0,698,699,3,58,29,0,699,700,5,26,0,0,700,702,1,0,0,0,701,677,
  	1,0,0,0,701,682,1,0,0,0,701,695,1,0,0,0,702,719,1,0,0,0,703,704,10,4,
  	0,0,704,710,7,17,0,0,705,711,3,122,61,0,706,707,5,7,0,0,707,708,3,58,
  	29,0,708,709,5,8,0,0,709,711,1,0,0,0,710,705,1,0,0,0,710,706,1,0,0,0,
  	711,718,1,0,0,0,712,713,10,3,0,0,713,714,5,25,0,0,714,715,3,58,29,0,715,
  	716,5,26,0,0,716,718,1,0,0,0,717,703,1,0,0,0,717,712,1,0,0,0,718,721,
  	1,0,0,0,719,717,1,0,0,0,719,720,1,0,0,0,720,101,1,0,0,0,721,719,1,0,0,
  	0,722,732,3,120,60,0,723,732,3,116,58,0,724,732,3,118,59,0,725,732,3,
  	18,9,0,726,732,3,104,52,0,727,732,5,105,0,0,728,732,3,106,53,0,729,732,
  	3,108,54,0,730,732,3,110,55,0,731,722,1,0,0,0,731,723,1,0,0,0,731,724,
  	1,0,0,0,731,725,1,0,0,0,731,726,1,0,0,0,731,727,1,0,0,0,731,728,1,0,0,
  	0,731,729,1,0,0,0,731,730,1,0,0,0,732,103,1,0,0,0,733,743,3,114,57,0,
  	734,743,3,112,56,0,735,743,5,104,0,0,736,743,5,102,0,0,737,743,5,110,
  	0,0,738,743,5,111,0,0,739,743,5,112,0,0,740,743,5,107,0,0,741,743,5,108,
  	0,0,742,733,1,0,0,0,742,734,1,0,0,0,742,735,1,0,0,0,742,736,1,0,0,0,742,
  	737,1,0,0,0,742,738,1,0,0,0,742,739,1,0,0,0,742,740,1,0,0,0,742,741,1,
  	0,0,0,743,105,1,0,0,0,744,747,5,33,0,0,745,746,5,106,0,0,746,748,5,31,
  	0,0,747,745,1,0,0,0,747,748,1,0,0,0,748,749,1,0,0,0,749,760,5,106,0,0,
  	750,751,5,94,0,0,751,752,5,106,0,0,752,753,5,29,0,0,753,754,5,109,0,0,
  	754,755,5,30,0,0,755,756,5,31,0,0,756,760,5,106,0,0,757,758,5,33,0,0,
  	758,760,3,100,50,0,759,744,1,0,0,0,759,750,1,0,0,0,759,757,1,0,0,0,760,
  	107,1,0,0,0,761,762,5,81,0,0,762,765,5,106,0,0,763,764,5,31,0,0,764,766,
  	5,106,0,0,765,763,1,0,0,0,765,766,1,0,0,0,766,109,1,0,0,0,767,768,5,94,
  	0,0,768,769,5,25,0,0,769,770,5,106,0,0,770,771,5,10,0,0,771,772,5,106,
  	0,0,772,773,5,26,0,0,773,111,1,0,0,0,774,775,7,18,0,0,775,113,1,0,0,0,
  	776,777,5,7,0,0,777,778,3,58,29,0,778,779,5,10,0,0,779,780,3,58,29,0,
  	780,781,5,10,0,0,781,782,3,58,29,0,782,783,5,8,0,0,783,115,1,0,0,0,784,
  	806,5,25,0,0,785,786,3,58,29,0,786,787,5,17,0,0,787,789,1,0,0,0,788,785,
  	1,0,0,0,788,789,1,0,0,0,789,790,1,0,0,0,790,800,3,58,29,0,791,795,5,10,
  	0,0,792,793,3,58,29,0,793,794,5,17,0,0,794,796,1,0,0,0,795,792,1,0,0,
  	0,795,796,1,0,0,0,796,797,1,0,0,0,797,799,3,58,29,0,798,791,1,0,0,0,799,
  	802,1,0,0,0,800,798,1,0,0,0,800,801,1,0,0,0,801,804,1,0,0,0,802,800,1,
  	0,0,0,803,805,5,10,0,0,804,803,1,0,0,0,804,805,1,0,0,0,805,807,1,0,0,
  	0,806,788,1,0,0,0,806,807,1,0,0,0,807,808,1,0,0,0,808,809,5,26,0,0,809,
  	117,1,0,0,0,810,827,5,18,0,0,811,812,3,58,29,0,812,813,5,17,0,0,813,821,
  	3,58,29,0,814,815,5,10,0,0,815,816,3,58,29,0,816,817,5,17,0,0,817,818,
  	3,58,29,0,818,820,1,0,0,0,819,814,1,0,0,0,820,823,1,0,0,0,821,819,1,0,
  	0,0,821,822,1,0,0,0,822,825,1,0,0,0,823,821,1,0,0,0,824,826,5,10,0,0,
  	825,824,1,0,0,0,825,826,1,0,0,0,826,828,1,0,0,0,827,811,1,0,0,0,827,828,
  	1,0,0,0,828,829,1,0,0,0,829,830,5,19,0,0,830,119,1,0,0,0,831,832,5,95,
  	0,0,832,833,5,106,0,0,833,842,5,7,0,0,834,839,3,58,29,0,835,836,5,10,
  	0,0,836,838,3,58,29,0,837,835,1,0,0,0,838,841,1,0,0,0,839,837,1,0,0,0,
  	839,840,1,0,0,0,840,843,1,0,0,0,841,839,1,0,0,0,842,834,1,0,0,0,842,843,
  	1,0,0,0,843,844,1,0,0,0,844,845,5,8,0,0,845,121,1,0,0,0,846,847,5,106,
  	0,0,847,123,1,0,0,0,88,134,136,181,184,190,196,205,210,213,220,222,228,
  	231,234,240,251,258,261,264,269,272,279,282,288,295,305,311,319,323,327,
  	350,364,374,380,385,396,405,414,431,436,444,459,473,484,495,506,517,528,
  	539,550,561,572,583,594,606,610,615,624,631,638,642,651,654,661,665,674,
  	682,690,695,701,710,717,719,731,742,747,759,765,788,795,800,804,806,821,
  	825,827,839,842
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  gscParserStaticData = staticData.release();
}

}

gscParser::gscParser(TokenStream *input) : gscParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

gscParser::gscParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  gscParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *gscParserStaticData->atn, gscParserStaticData->decisionToDFA, gscParserStaticData->sharedContextCache, options);
}

gscParser::~gscParser() {
  delete _interpreter;
}

const atn::ATN& gscParser::getATN() const {
  return *gscParserStaticData->atn;
}

std::string gscParser::getGrammarFileName() const {
  return "gsc.g4";
}

const std::vector<std::string>& gscParser::getRuleNames() const {
  return gscParserStaticData->ruleNames;
}

const dfa::Vocabulary& gscParser::getVocabulary() const {
  return gscParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView gscParser::getSerializedATN() const {
  return gscParserStaticData->serializedATN;
}


//----------------- ProgContext ------------------------------------------------------------------

gscParser::ProgContext::ProgContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::ProgContext::EOF() {
  return getToken(gscParser::EOF, 0);
}

std::vector<gscParser::FunctionContext *> gscParser::ProgContext::function() {
  return getRuleContexts<gscParser::FunctionContext>();
}

gscParser::FunctionContext* gscParser::ProgContext::function(size_t i) {
  return getRuleContext<gscParser::FunctionContext>(i);
}

std::vector<gscParser::IncludeContext *> gscParser::ProgContext::include() {
  return getRuleContexts<gscParser::IncludeContext>();
}

gscParser::IncludeContext* gscParser::ProgContext::include(size_t i) {
  return getRuleContext<gscParser::IncludeContext>(i);
}

std::vector<gscParser::PrecacheContext *> gscParser::ProgContext::precache() {
  return getRuleContexts<gscParser::PrecacheContext>();
}

gscParser::PrecacheContext* gscParser::ProgContext::precache(size_t i) {
  return getRuleContext<gscParser::PrecacheContext>(i);
}

std::vector<gscParser::Using_animtreeContext *> gscParser::ProgContext::using_animtree() {
  return getRuleContexts<gscParser::Using_animtreeContext>();
}

gscParser::Using_animtreeContext* gscParser::ProgContext::using_animtree(size_t i) {
  return getRuleContext<gscParser::Using_animtreeContext>(i);
}

std::vector<gscParser::NamespaceContext *> gscParser::ProgContext::namespace_() {
  return getRuleContexts<gscParser::NamespaceContext>();
}

gscParser::NamespaceContext* gscParser::ProgContext::namespace_(size_t i) {
  return getRuleContext<gscParser::NamespaceContext>(i);
}

std::vector<gscParser::FilenamespaceContext *> gscParser::ProgContext::filenamespace() {
  return getRuleContexts<gscParser::FilenamespaceContext>();
}

gscParser::FilenamespaceContext* gscParser::ProgContext::filenamespace(size_t i) {
  return getRuleContext<gscParser::FilenamespaceContext>(i);
}

std::vector<gscParser::ConstexprContext *> gscParser::ProgContext::constexpr_() {
  return getRuleContexts<gscParser::ConstexprContext>();
}

gscParser::ConstexprContext* gscParser::ProgContext::constexpr_(size_t i) {
  return getRuleContext<gscParser::ConstexprContext>(i);
}

std::vector<gscParser::Class_defContext *> gscParser::ProgContext::class_def() {
  return getRuleContexts<gscParser::Class_defContext>();
}

gscParser::Class_defContext* gscParser::ProgContext::class_def(size_t i) {
  return getRuleContext<gscParser::Class_defContext>(i);
}


size_t gscParser::ProgContext::getRuleIndex() const {
  return gscParser::RuleProg;
}


std::any gscParser::ProgContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitProg(this);
  else
    return visitor->visitChildren(this);
}

gscParser::ProgContext* gscParser::prog() {
  ProgContext *_localctx = _tracker.createInstance<ProgContext>(_ctx, getState());
  enterRule(_localctx, 0, gscParser::RuleProg);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(136);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 299989726) != 0) || _la == gscParser::IDENTIFIER) {
      setState(134);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case gscParser::T__0: {
          setState(124);
          match(gscParser::T__0);
          break;
        }

        case gscParser::T__1: {
          setState(125);
          match(gscParser::T__1);
          break;
        }

        case gscParser::T__6:
        case gscParser::T__20:
        case gscParser::T__21:
        case gscParser::T__22:
        case gscParser::T__23:
        case gscParser::T__27:
        case gscParser::IDENTIFIER: {
          setState(126);
          function();
          break;
        }

        case gscParser::T__2:
        case gscParser::T__3: {
          setState(127);
          include();
          break;
        }

        case gscParser::T__8: {
          setState(128);
          precache();
          break;
        }

        case gscParser::T__5: {
          setState(129);
          using_animtree();
          break;
        }

        case gscParser::T__10: {
          setState(130);
          namespace_();
          break;
        }

        case gscParser::T__11: {
          setState(131);
          filenamespace();
          break;
        }

        case gscParser::T__12:
        case gscParser::T__13: {
          setState(132);
          constexpr_();
          break;
        }

        case gscParser::T__15: {
          setState(133);
          class_def();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(138);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(139);
    match(gscParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- IncludeContext ------------------------------------------------------------------

gscParser::IncludeContext::IncludeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::IncludeContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

tree::TerminalNode* gscParser::IncludeContext::PATH() {
  return getToken(gscParser::PATH, 0);
}


size_t gscParser::IncludeContext::getRuleIndex() const {
  return gscParser::RuleInclude;
}


std::any gscParser::IncludeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitInclude(this);
  else
    return visitor->visitChildren(this);
}

gscParser::IncludeContext* gscParser::include() {
  IncludeContext *_localctx = _tracker.createInstance<IncludeContext>(_ctx, getState());
  enterRule(_localctx, 2, gscParser::RuleInclude);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(141);
    _la = _input->LA(1);
    if (!(_la == gscParser::T__2

    || _la == gscParser::T__3)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(142);
    _la = _input->LA(1);
    if (!(_la == gscParser::IDENTIFIER

    || _la == gscParser::PATH)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(143);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Using_animtreeContext ------------------------------------------------------------------

gscParser::Using_animtreeContext::Using_animtreeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::Using_animtreeContext::STRING() {
  return getToken(gscParser::STRING, 0);
}


size_t gscParser::Using_animtreeContext::getRuleIndex() const {
  return gscParser::RuleUsing_animtree;
}


std::any gscParser::Using_animtreeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitUsing_animtree(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Using_animtreeContext* gscParser::using_animtree() {
  Using_animtreeContext *_localctx = _tracker.createInstance<Using_animtreeContext>(_ctx, getState());
  enterRule(_localctx, 4, gscParser::RuleUsing_animtree);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(145);
    match(gscParser::T__5);
    setState(146);
    match(gscParser::T__6);
    setState(147);
    match(gscParser::STRING);
    setState(148);
    match(gscParser::T__7);
    setState(149);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PrecacheContext ------------------------------------------------------------------

gscParser::PrecacheContext::PrecacheContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::PrecacheContext::STRING() {
  return getTokens(gscParser::STRING);
}

tree::TerminalNode* gscParser::PrecacheContext::STRING(size_t i) {
  return getToken(gscParser::STRING, i);
}


size_t gscParser::PrecacheContext::getRuleIndex() const {
  return gscParser::RulePrecache;
}


std::any gscParser::PrecacheContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitPrecache(this);
  else
    return visitor->visitChildren(this);
}

gscParser::PrecacheContext* gscParser::precache() {
  PrecacheContext *_localctx = _tracker.createInstance<PrecacheContext>(_ctx, getState());
  enterRule(_localctx, 6, gscParser::RulePrecache);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(151);
    match(gscParser::T__8);
    setState(152);
    match(gscParser::T__6);
    setState(153);
    match(gscParser::STRING);
    setState(154);
    match(gscParser::T__9);
    setState(155);
    match(gscParser::STRING);
    setState(156);
    match(gscParser::T__7);
    setState(157);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- NamespaceContext ------------------------------------------------------------------

gscParser::NamespaceContext::NamespaceContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::NamespaceContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}


size_t gscParser::NamespaceContext::getRuleIndex() const {
  return gscParser::RuleNamespace;
}


std::any gscParser::NamespaceContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitNamespace(this);
  else
    return visitor->visitChildren(this);
}

gscParser::NamespaceContext* gscParser::namespace_() {
  NamespaceContext *_localctx = _tracker.createInstance<NamespaceContext>(_ctx, getState());
  enterRule(_localctx, 8, gscParser::RuleNamespace);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(159);
    match(gscParser::T__10);
    setState(160);
    match(gscParser::IDENTIFIER);
    setState(161);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FilenamespaceContext ------------------------------------------------------------------

gscParser::FilenamespaceContext::FilenamespaceContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::FilenamespaceContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

tree::TerminalNode* gscParser::FilenamespaceContext::PATH() {
  return getToken(gscParser::PATH, 0);
}


size_t gscParser::FilenamespaceContext::getRuleIndex() const {
  return gscParser::RuleFilenamespace;
}


std::any gscParser::FilenamespaceContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitFilenamespace(this);
  else
    return visitor->visitChildren(this);
}

gscParser::FilenamespaceContext* gscParser::filenamespace() {
  FilenamespaceContext *_localctx = _tracker.createInstance<FilenamespaceContext>(_ctx, getState());
  enterRule(_localctx, 10, gscParser::RuleFilenamespace);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(163);
    match(gscParser::T__11);
    setState(164);
    _la = _input->LA(1);
    if (!(_la == gscParser::IDENTIFIER

    || _la == gscParser::PATH)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(165);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ConstexprContext ------------------------------------------------------------------

gscParser::ConstexprContext::ConstexprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::ConstexprContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

gscParser::ExpressionContext* gscParser::ConstexprContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::ConstexprContext::getRuleIndex() const {
  return gscParser::RuleConstexpr;
}


std::any gscParser::ConstexprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitConstexpr(this);
  else
    return visitor->visitChildren(this);
}

gscParser::ConstexprContext* gscParser::constexpr_() {
  ConstexprContext *_localctx = _tracker.createInstance<ConstexprContext>(_ctx, getState());
  enterRule(_localctx, 12, gscParser::RuleConstexpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(167);
    _la = _input->LA(1);
    if (!(_la == gscParser::T__12

    || _la == gscParser::T__13)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(168);
    match(gscParser::IDENTIFIER);
    setState(169);
    match(gscParser::T__14);
    setState(170);
    expression();
    setState(171);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Class_defContext ------------------------------------------------------------------

gscParser::Class_defContext::Class_defContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Class_defContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Class_defContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

std::vector<gscParser::Class_varContext *> gscParser::Class_defContext::class_var() {
  return getRuleContexts<gscParser::Class_varContext>();
}

gscParser::Class_varContext* gscParser::Class_defContext::class_var(size_t i) {
  return getRuleContext<gscParser::Class_varContext>(i);
}

std::vector<gscParser::FunctionContext *> gscParser::Class_defContext::function() {
  return getRuleContexts<gscParser::FunctionContext>();
}

gscParser::FunctionContext* gscParser::Class_defContext::function(size_t i) {
  return getRuleContext<gscParser::FunctionContext>(i);
}


size_t gscParser::Class_defContext::getRuleIndex() const {
  return gscParser::RuleClass_def;
}


std::any gscParser::Class_defContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitClass_def(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Class_defContext* gscParser::class_def() {
  Class_defContext *_localctx = _tracker.createInstance<Class_defContext>(_ctx, getState());
  enterRule(_localctx, 14, gscParser::RuleClass_def);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(173);
    match(gscParser::T__15);
    setState(174);
    match(gscParser::IDENTIFIER);
    setState(184);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__16) {
      setState(175);
      match(gscParser::T__16);
      setState(176);
      match(gscParser::IDENTIFIER);
      setState(181);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == gscParser::T__9) {
        setState(177);
        match(gscParser::T__9);
        setState(178);
        match(gscParser::IDENTIFIER);
        setState(183);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(186);
    match(gscParser::T__17);
    setState(190);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == gscParser::T__19) {
      setState(187);
      class_var();
      setState(192);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(196);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 299892864) != 0) || _la == gscParser::IDENTIFIER) {
      setState(193);
      function();
      setState(198);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(199);
    match(gscParser::T__18);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Class_varContext ------------------------------------------------------------------

gscParser::Class_varContext::Class_varContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::Class_varContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

gscParser::ExpressionContext* gscParser::Class_varContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Class_varContext::getRuleIndex() const {
  return gscParser::RuleClass_var;
}


std::any gscParser::Class_varContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitClass_var(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Class_varContext* gscParser::class_var() {
  Class_varContext *_localctx = _tracker.createInstance<Class_varContext>(_ctx, getState());
  enterRule(_localctx, 16, gscParser::RuleClass_var);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(201);
    match(gscParser::T__19);
    setState(202);
    match(gscParser::IDENTIFIER);
    setState(205);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__14) {
      setState(203);
      match(gscParser::T__14);
      setState(204);
      expression();
    }
    setState(207);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FunctionContext ------------------------------------------------------------------

gscParser::FunctionContext::FunctionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Param_listContext* gscParser::FunctionContext::param_list() {
  return getRuleContext<gscParser::Param_listContext>(0);
}

gscParser::Statement_blockContext* gscParser::FunctionContext::statement_block() {
  return getRuleContext<gscParser::Statement_blockContext>(0);
}

std::vector<tree::TerminalNode *> gscParser::FunctionContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::FunctionContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

gscParser::Detour_infoContext* gscParser::FunctionContext::detour_info() {
  return getRuleContext<gscParser::Detour_infoContext>(0);
}

gscParser::NumberContext* gscParser::FunctionContext::number() {
  return getRuleContext<gscParser::NumberContext>(0);
}


size_t gscParser::FunctionContext::getRuleIndex() const {
  return gscParser::RuleFunction;
}


std::any gscParser::FunctionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitFunction(this);
  else
    return visitor->visitChildren(this);
}

gscParser::FunctionContext* gscParser::function() {
  FunctionContext *_localctx = _tracker.createInstance<FunctionContext>(_ctx, getState());
  enterRule(_localctx, 18, gscParser::RuleFunction);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(210);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__20) {
      setState(209);
      match(gscParser::T__20);
    }
    setState(213);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__21) {
      setState(212);
      match(gscParser::T__21);
    }
    setState(222);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__22) {
      setState(215);
      match(gscParser::T__22);
      setState(220);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 9, _ctx)) {
      case 1: {
        setState(216);
        match(gscParser::T__6);
        setState(217);
        number();
        setState(218);
        match(gscParser::T__7);
        break;
      }

      default:
        break;
      }
    }
    setState(228);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__23) {
      setState(224);
      match(gscParser::T__23);
      setState(225);
      match(gscParser::T__24);
      setState(226);
      match(gscParser::IDENTIFIER);
      setState(227);
      match(gscParser::T__25);
    }
    setState(231);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__27) {
      setState(230);
      detour_info();
    }
    setState(234);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::IDENTIFIER) {
      setState(233);
      match(gscParser::IDENTIFIER);
    }
    setState(236);
    match(gscParser::T__6);
    setState(237);
    param_list();
    setState(238);
    match(gscParser::T__7);
    setState(240);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__26) {
      setState(239);
      match(gscParser::T__26);
    }
    setState(242);
    statement_block();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Detour_infoContext ------------------------------------------------------------------

gscParser::Detour_infoContext::Detour_infoContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Detour_infoContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Detour_infoContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

tree::TerminalNode* gscParser::Detour_infoContext::PATH() {
  return getToken(gscParser::PATH, 0);
}


size_t gscParser::Detour_infoContext::getRuleIndex() const {
  return gscParser::RuleDetour_info;
}


std::any gscParser::Detour_infoContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitDetour_info(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Detour_infoContext* gscParser::detour_info() {
  Detour_infoContext *_localctx = _tracker.createInstance<Detour_infoContext>(_ctx, getState());
  enterRule(_localctx, 20, gscParser::RuleDetour_info);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(244);
    match(gscParser::T__27);
    setState(245);
    match(gscParser::IDENTIFIER);
    setState(251);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__28) {
      setState(246);
      match(gscParser::T__28);
      setState(247);
      match(gscParser::PATH);
      setState(248);
      match(gscParser::T__29);
      setState(249);
      match(gscParser::T__30);
      setState(250);
      match(gscParser::IDENTIFIER);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Param_listContext ------------------------------------------------------------------

gscParser::Param_listContext::Param_listContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::Param_valContext *> gscParser::Param_listContext::param_val() {
  return getRuleContexts<gscParser::Param_valContext>();
}

gscParser::Param_valContext* gscParser::Param_listContext::param_val(size_t i) {
  return getRuleContext<gscParser::Param_valContext>(i);
}


size_t gscParser::Param_listContext::getRuleIndex() const {
  return gscParser::RuleParam_list;
}


std::any gscParser::Param_listContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitParam_list(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Param_listContext* gscParser::param_list() {
  Param_listContext *_localctx = _tracker.createInstance<Param_listContext>(_ctx, getState());
  enterRule(_localctx, 22, gscParser::RuleParam_list);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(261);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 30064771072) != 0) || _la == gscParser::IDENTIFIER) {
      setState(253);
      param_val();
      setState(258);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == gscParser::T__9) {
        setState(254);
        match(gscParser::T__9);
        setState(255);
        param_val();
        setState(260);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Param_valContext ------------------------------------------------------------------

gscParser::Param_valContext::Param_valContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::Param_valContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

gscParser::ExpressionContext* gscParser::Param_valContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Param_valContext::getRuleIndex() const {
  return gscParser::RuleParam_val;
}


std::any gscParser::Param_valContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitParam_val(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Param_valContext* gscParser::param_val() {
  Param_valContext *_localctx = _tracker.createInstance<Param_valContext>(_ctx, getState());
  enterRule(_localctx, 24, gscParser::RuleParam_val);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(272);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case gscParser::T__31:
      case gscParser::T__32:
      case gscParser::IDENTIFIER: {
        enterOuterAlt(_localctx, 1);
        setState(264);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == gscParser::T__31

        || _la == gscParser::T__32) {
          setState(263);
          _la = _input->LA(1);
          if (!(_la == gscParser::T__31

          || _la == gscParser::T__32)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
        }
        setState(266);
        match(gscParser::IDENTIFIER);
        setState(269);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == gscParser::T__14) {
          setState(267);
          match(gscParser::T__14);
          setState(268);
          expression();
        }
        break;
      }

      case gscParser::T__33: {
        enterOuterAlt(_localctx, 2);
        setState(271);
        match(gscParser::T__33);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Expression_listContext ------------------------------------------------------------------

gscParser::Expression_listContext::Expression_listContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::ExpressionContext *> gscParser::Expression_listContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::Expression_listContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::Expression_listContext::getRuleIndex() const {
  return gscParser::RuleExpression_list;
}


std::any gscParser::Expression_listContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression_list(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Expression_listContext* gscParser::expression_list() {
  Expression_listContext *_localctx = _tracker.createInstance<Expression_listContext>(_ctx, getState());
  enterRule(_localctx, 26, gscParser::RuleExpression_list);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(282);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(274);
      expression();
      setState(279);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == gscParser::T__9) {
        setState(275);
        match(gscParser::T__9);
        setState(276);
        expression();
        setState(281);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_blockContext ------------------------------------------------------------------

gscParser::Statement_blockContext::Statement_blockContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::StatementContext *> gscParser::Statement_blockContext::statement() {
  return getRuleContexts<gscParser::StatementContext>();
}

gscParser::StatementContext* gscParser::Statement_blockContext::statement(size_t i) {
  return getRuleContext<gscParser::StatementContext>(i);
}


size_t gscParser::Statement_blockContext::getRuleIndex() const {
  return gscParser::RuleStatement_block;
}


std::any gscParser::Statement_blockContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_block(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_blockContext* gscParser::statement_block() {
  Statement_blockContext *_localctx = _tracker.createInstance<Statement_blockContext>(_ctx, getState());
  enterRule(_localctx, 28, gscParser::RuleStatement_block);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(284);
    match(gscParser::T__17);
    setState(288);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1096737412612226) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4026427295) != 0)) {
      setState(285);
      statement();
      setState(290);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(291);
    match(gscParser::T__18);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementContext ------------------------------------------------------------------

gscParser::StatementContext::StatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Statement_blockContext* gscParser::StatementContext::statement_block() {
  return getRuleContext<gscParser::Statement_blockContext>(0);
}

gscParser::Statement_dev_blockContext* gscParser::StatementContext::statement_dev_block() {
  return getRuleContext<gscParser::Statement_dev_blockContext>(0);
}

gscParser::Statement_forContext* gscParser::StatementContext::statement_for() {
  return getRuleContext<gscParser::Statement_forContext>(0);
}

gscParser::Statement_ifContext* gscParser::StatementContext::statement_if() {
  return getRuleContext<gscParser::Statement_ifContext>(0);
}

gscParser::Statement_whileContext* gscParser::StatementContext::statement_while() {
  return getRuleContext<gscParser::Statement_whileContext>(0);
}

gscParser::Statement_foreachContext* gscParser::StatementContext::statement_foreach() {
  return getRuleContext<gscParser::Statement_foreachContext>(0);
}

gscParser::Statement_instContext* gscParser::StatementContext::statement_inst() {
  return getRuleContext<gscParser::Statement_instContext>(0);
}

gscParser::Statement_switchContext* gscParser::StatementContext::statement_switch() {
  return getRuleContext<gscParser::Statement_switchContext>(0);
}

tree::TerminalNode* gscParser::StatementContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}


size_t gscParser::StatementContext::getRuleIndex() const {
  return gscParser::RuleStatement;
}


std::any gscParser::StatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement(this);
  else
    return visitor->visitChildren(this);
}

gscParser::StatementContext* gscParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 30, gscParser::RuleStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(295);
    _errHandler->sync(this);

    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 24, _ctx)) {
    case 1: {
      setState(293);
      match(gscParser::IDENTIFIER);
      setState(294);
      match(gscParser::T__16);
      break;
    }

    default:
      break;
    }
    setState(305);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 25, _ctx)) {
    case 1: {
      setState(297);
      statement_block();
      break;
    }

    case 2: {
      setState(298);
      statement_dev_block();
      break;
    }

    case 3: {
      setState(299);
      statement_for();
      break;
    }

    case 4: {
      setState(300);
      statement_if();
      break;
    }

    case 5: {
      setState(301);
      statement_while();
      break;
    }

    case 6: {
      setState(302);
      statement_foreach();
      break;
    }

    case 7: {
      setState(303);
      statement_inst();
      break;
    }

    case 8: {
      setState(304);
      statement_switch();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_dev_blockContext ------------------------------------------------------------------

gscParser::Statement_dev_blockContext::Statement_dev_blockContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::StatementContext *> gscParser::Statement_dev_blockContext::statement() {
  return getRuleContexts<gscParser::StatementContext>();
}

gscParser::StatementContext* gscParser::Statement_dev_blockContext::statement(size_t i) {
  return getRuleContext<gscParser::StatementContext>(i);
}


size_t gscParser::Statement_dev_blockContext::getRuleIndex() const {
  return gscParser::RuleStatement_dev_block;
}


std::any gscParser::Statement_dev_blockContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_dev_block(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_dev_blockContext* gscParser::statement_dev_block() {
  Statement_dev_blockContext *_localctx = _tracker.createInstance<Statement_dev_blockContext>(_ctx, getState());
  enterRule(_localctx, 32, gscParser::RuleStatement_dev_block);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(307);
    match(gscParser::T__0);
    setState(311);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1096737412612226) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4026427295) != 0)) {
      setState(308);
      statement();
      setState(313);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(314);
    match(gscParser::T__1);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_forContext ------------------------------------------------------------------

gscParser::Statement_forContext::Statement_forContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::StatementContext* gscParser::Statement_forContext::statement() {
  return getRuleContext<gscParser::StatementContext>(0);
}

std::vector<gscParser::ExpressionContext *> gscParser::Statement_forContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::Statement_forContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::Statement_forContext::getRuleIndex() const {
  return gscParser::RuleStatement_for;
}


std::any gscParser::Statement_forContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_for(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_forContext* gscParser::statement_for() {
  Statement_forContext *_localctx = _tracker.createInstance<Statement_forContext>(_ctx, getState());
  enterRule(_localctx, 34, gscParser::RuleStatement_for);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(316);
    match(gscParser::T__34);
    setState(317);
    match(gscParser::T__6);
    setState(319);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(318);
      expression();
    }
    setState(321);
    match(gscParser::T__4);
    setState(323);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(322);
      expression();
    }
    setState(325);
    match(gscParser::T__4);
    setState(327);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(326);
      expression();
    }
    setState(329);
    match(gscParser::T__7);
    setState(330);
    statement();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_whileContext ------------------------------------------------------------------

gscParser::Statement_whileContext::Statement_whileContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::ExpressionContext* gscParser::Statement_whileContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}

gscParser::StatementContext* gscParser::Statement_whileContext::statement() {
  return getRuleContext<gscParser::StatementContext>(0);
}


size_t gscParser::Statement_whileContext::getRuleIndex() const {
  return gscParser::RuleStatement_while;
}


std::any gscParser::Statement_whileContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_while(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_whileContext* gscParser::statement_while() {
  Statement_whileContext *_localctx = _tracker.createInstance<Statement_whileContext>(_ctx, getState());
  enterRule(_localctx, 36, gscParser::RuleStatement_while);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(332);
    match(gscParser::T__35);
    setState(333);
    match(gscParser::T__6);
    setState(334);
    expression();
    setState(335);
    match(gscParser::T__7);
    setState(336);
    statement();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_dowhileContext ------------------------------------------------------------------

gscParser::Statement_dowhileContext::Statement_dowhileContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::StatementContext* gscParser::Statement_dowhileContext::statement() {
  return getRuleContext<gscParser::StatementContext>(0);
}

gscParser::ExpressionContext* gscParser::Statement_dowhileContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Statement_dowhileContext::getRuleIndex() const {
  return gscParser::RuleStatement_dowhile;
}


std::any gscParser::Statement_dowhileContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_dowhile(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_dowhileContext* gscParser::statement_dowhile() {
  Statement_dowhileContext *_localctx = _tracker.createInstance<Statement_dowhileContext>(_ctx, getState());
  enterRule(_localctx, 38, gscParser::RuleStatement_dowhile);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(338);
    match(gscParser::T__36);
    setState(339);
    statement();
    setState(340);
    match(gscParser::T__35);
    setState(341);
    match(gscParser::T__6);
    setState(342);
    expression();
    setState(343);
    match(gscParser::T__7);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_foreachContext ------------------------------------------------------------------

gscParser::Statement_foreachContext::Statement_foreachContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Statement_foreachContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Statement_foreachContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

gscParser::ExpressionContext* gscParser::Statement_foreachContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}

gscParser::StatementContext* gscParser::Statement_foreachContext::statement() {
  return getRuleContext<gscParser::StatementContext>(0);
}


size_t gscParser::Statement_foreachContext::getRuleIndex() const {
  return gscParser::RuleStatement_foreach;
}


std::any gscParser::Statement_foreachContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_foreach(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_foreachContext* gscParser::statement_foreach() {
  Statement_foreachContext *_localctx = _tracker.createInstance<Statement_foreachContext>(_ctx, getState());
  enterRule(_localctx, 40, gscParser::RuleStatement_foreach);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(345);
    match(gscParser::T__37);
    setState(346);
    match(gscParser::T__6);
    setState(347);
    match(gscParser::IDENTIFIER);
    setState(350);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__9) {
      setState(348);
      match(gscParser::T__9);
      setState(349);
      match(gscParser::IDENTIFIER);
    }
    setState(352);
    match(gscParser::T__38);
    setState(353);
    expression();
    setState(354);
    match(gscParser::T__7);
    setState(355);
    statement();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_ifContext ------------------------------------------------------------------

gscParser::Statement_ifContext::Statement_ifContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::ExpressionContext* gscParser::Statement_ifContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}

std::vector<gscParser::StatementContext *> gscParser::Statement_ifContext::statement() {
  return getRuleContexts<gscParser::StatementContext>();
}

gscParser::StatementContext* gscParser::Statement_ifContext::statement(size_t i) {
  return getRuleContext<gscParser::StatementContext>(i);
}


size_t gscParser::Statement_ifContext::getRuleIndex() const {
  return gscParser::RuleStatement_if;
}


std::any gscParser::Statement_ifContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_if(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_ifContext* gscParser::statement_if() {
  Statement_ifContext *_localctx = _tracker.createInstance<Statement_ifContext>(_ctx, getState());
  enterRule(_localctx, 42, gscParser::RuleStatement_if);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(357);
    match(gscParser::T__39);
    setState(358);
    match(gscParser::T__6);
    setState(359);
    expression();
    setState(360);
    match(gscParser::T__7);
    setState(361);
    statement();
    setState(364);
    _errHandler->sync(this);

    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 31, _ctx)) {
    case 1: {
      setState(362);
      match(gscParser::T__40);
      setState(363);
      statement();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_switchContext ------------------------------------------------------------------

gscParser::Statement_switchContext::Statement_switchContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::ExpressionContext* gscParser::Statement_switchContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}

std::vector<gscParser::Const_exprContext *> gscParser::Statement_switchContext::const_expr() {
  return getRuleContexts<gscParser::Const_exprContext>();
}

gscParser::Const_exprContext* gscParser::Statement_switchContext::const_expr(size_t i) {
  return getRuleContext<gscParser::Const_exprContext>(i);
}

std::vector<gscParser::StatementContext *> gscParser::Statement_switchContext::statement() {
  return getRuleContexts<gscParser::StatementContext>();
}

gscParser::StatementContext* gscParser::Statement_switchContext::statement(size_t i) {
  return getRuleContext<gscParser::StatementContext>(i);
}


size_t gscParser::Statement_switchContext::getRuleIndex() const {
  return gscParser::RuleStatement_switch;
}


std::any gscParser::Statement_switchContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_switch(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_switchContext* gscParser::statement_switch() {
  Statement_switchContext *_localctx = _tracker.createInstance<Statement_switchContext>(_ctx, getState());
  enterRule(_localctx, 44, gscParser::RuleStatement_switch);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(366);
    match(gscParser::T__41);
    setState(367);
    match(gscParser::T__6);
    setState(368);
    expression();
    setState(369);
    match(gscParser::T__7);
    setState(370);
    match(gscParser::T__17);
    setState(383); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(374);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case gscParser::T__42: {
          setState(371);
          match(gscParser::T__42);
          setState(372);
          const_expr();
          break;
        }

        case gscParser::T__43: {
          setState(373);
          match(gscParser::T__43);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(376);
      match(gscParser::T__16);
      setState(380);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 1096737412612226) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 81)) & 4026427295) != 0)) {
        setState(377);
        statement();
        setState(382);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
      setState(385); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == gscParser::T__42

    || _la == gscParser::T__43);
    setState(387);
    match(gscParser::T__18);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Statement_instContext ------------------------------------------------------------------

gscParser::Statement_instContext::Statement_instContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::ExpressionContext* gscParser::Statement_instContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}

gscParser::Operator_instContext* gscParser::Statement_instContext::operator_inst() {
  return getRuleContext<gscParser::Operator_instContext>(0);
}

gscParser::Statement_dowhileContext* gscParser::Statement_instContext::statement_dowhile() {
  return getRuleContext<gscParser::Statement_dowhileContext>(0);
}

gscParser::Function_call_expContext* gscParser::Statement_instContext::function_call_exp() {
  return getRuleContext<gscParser::Function_call_expContext>(0);
}

gscParser::Nop_defContext* gscParser::Statement_instContext::nop_def() {
  return getRuleContext<gscParser::Nop_defContext>(0);
}

gscParser::Devop_defContext* gscParser::Statement_instContext::devop_def() {
  return getRuleContext<gscParser::Devop_defContext>(0);
}

gscParser::Array_unpackContext* gscParser::Statement_instContext::array_unpack() {
  return getRuleContext<gscParser::Array_unpackContext>(0);
}


size_t gscParser::Statement_instContext::getRuleIndex() const {
  return gscParser::RuleStatement_inst;
}


std::any gscParser::Statement_instContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStatement_inst(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Statement_instContext* gscParser::statement_inst() {
  Statement_instContext *_localctx = _tracker.createInstance<Statement_instContext>(_ctx, getState());
  enterRule(_localctx, 46, gscParser::RuleStatement_inst);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(396);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 35, _ctx)) {
    case 1: {
      setState(389);
      expression();
      break;
    }

    case 2: {
      setState(390);
      operator_inst();
      break;
    }

    case 3: {
      setState(391);
      statement_dowhile();
      break;
    }

    case 4: {
      setState(392);
      function_call_exp();
      break;
    }

    case 5: {
      setState(393);
      nop_def();
      break;
    }

    case 6: {
      setState(394);
      devop_def();
      break;
    }

    case 7: {
      setState(395);
      array_unpack();
      break;
    }

    default:
      break;
    }
    setState(398);
    match(gscParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Nop_defContext ------------------------------------------------------------------

gscParser::Nop_defContext::Nop_defContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::NumberContext* gscParser::Nop_defContext::number() {
  return getRuleContext<gscParser::NumberContext>(0);
}


size_t gscParser::Nop_defContext::getRuleIndex() const {
  return gscParser::RuleNop_def;
}


std::any gscParser::Nop_defContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitNop_def(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Nop_defContext* gscParser::nop_def() {
  Nop_defContext *_localctx = _tracker.createInstance<Nop_defContext>(_ctx, getState());
  enterRule(_localctx, 48, gscParser::RuleNop_def);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(400);
    _la = _input->LA(1);
    if (!(_la == gscParser::T__44

    || _la == gscParser::T__45)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(405);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__6) {
      setState(401);
      match(gscParser::T__6);
      setState(402);
      number();
      setState(403);
      match(gscParser::T__7);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Devop_defContext ------------------------------------------------------------------

gscParser::Devop_defContext::Devop_defContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::NumberContext* gscParser::Devop_defContext::number() {
  return getRuleContext<gscParser::NumberContext>(0);
}


size_t gscParser::Devop_defContext::getRuleIndex() const {
  return gscParser::RuleDevop_def;
}


std::any gscParser::Devop_defContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitDevop_def(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Devop_defContext* gscParser::devop_def() {
  Devop_defContext *_localctx = _tracker.createInstance<Devop_defContext>(_ctx, getState());
  enterRule(_localctx, 50, gscParser::RuleDevop_def);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(407);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 985162418487296) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(408);
    match(gscParser::T__6);
    setState(409);
    number();
    setState(410);
    match(gscParser::T__7);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Function_componentContext ------------------------------------------------------------------

gscParser::Function_componentContext::Function_componentContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Function_componentContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Function_componentContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

gscParser::ExpressionContext* gscParser::Function_componentContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Function_componentContext::getRuleIndex() const {
  return gscParser::RuleFunction_component;
}


std::any gscParser::Function_componentContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitFunction_component(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Function_componentContext* gscParser::function_component() {
  Function_componentContext *_localctx = _tracker.createInstance<Function_componentContext>(_ctx, getState());
  enterRule(_localctx, 52, gscParser::RuleFunction_component);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(431);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 38, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(414);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 37, _ctx)) {
      case 1: {
        setState(412);
        match(gscParser::IDENTIFIER);
        setState(413);
        match(gscParser::T__30);
        break;
      }

      default:
        break;
      }
      setState(416);
      match(gscParser::IDENTIFIER);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(417);
      match(gscParser::T__24);
      setState(418);
      match(gscParser::T__24);
      setState(419);
      expression();
      setState(420);
      match(gscParser::T__25);
      setState(421);
      match(gscParser::T__25);
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(423);
      match(gscParser::T__24);
      setState(424);
      match(gscParser::T__24);
      setState(425);
      expression();
      setState(426);
      match(gscParser::T__25);
      setState(427);
      match(gscParser::T__25);
      setState(428);
      match(gscParser::T__49);
      setState(429);
      match(gscParser::IDENTIFIER);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Operator_instContext ------------------------------------------------------------------

gscParser::Operator_instContext::Operator_instContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::Operator_instContext::BUILTIN() {
  return getToken(gscParser::BUILTIN, 0);
}

tree::TerminalNode* gscParser::Operator_instContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

gscParser::ExpressionContext* gscParser::Operator_instContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Operator_instContext::getRuleIndex() const {
  return gscParser::RuleOperator_inst;
}


std::any gscParser::Operator_instContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitOperator_inst(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Operator_instContext* gscParser::operator_inst() {
  Operator_instContext *_localctx = _tracker.createInstance<Operator_instContext>(_ctx, getState());
  enterRule(_localctx, 54, gscParser::RuleOperator_inst);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(433);
    match(gscParser::BUILTIN);
    setState(436);
    _errHandler->sync(this);

    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 39, _ctx)) {
    case 1: {
      setState(434);
      match(gscParser::IDENTIFIER);
      break;
    }

    case 2: {
      setState(435);
      expression();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Array_unpackContext ------------------------------------------------------------------

gscParser::Array_unpackContext::Array_unpackContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Array_unpackContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Array_unpackContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

gscParser::ExpressionContext* gscParser::Array_unpackContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Array_unpackContext::getRuleIndex() const {
  return gscParser::RuleArray_unpack;
}


std::any gscParser::Array_unpackContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitArray_unpack(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Array_unpackContext* gscParser::array_unpack() {
  Array_unpackContext *_localctx = _tracker.createInstance<Array_unpackContext>(_ctx, getState());
  enterRule(_localctx, 56, gscParser::RuleArray_unpack);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(438);
    match(gscParser::T__24);
    setState(439);
    match(gscParser::IDENTIFIER);
    setState(444);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == gscParser::T__9) {
      setState(440);
      match(gscParser::T__9);
      setState(441);
      match(gscParser::IDENTIFIER);
      setState(446);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(447);
    match(gscParser::T__25);
    setState(448);
    match(gscParser::T__14);
    setState(449);
    expression();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExpressionContext ------------------------------------------------------------------

gscParser::ExpressionContext::ExpressionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Set_expressionContext* gscParser::ExpressionContext::set_expression() {
  return getRuleContext<gscParser::Set_expressionContext>(0);
}

gscParser::Expression0Context* gscParser::ExpressionContext::expression0() {
  return getRuleContext<gscParser::Expression0Context>(0);
}

std::vector<gscParser::ExpressionContext *> gscParser::ExpressionContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::ExpressionContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::ExpressionContext::getRuleIndex() const {
  return gscParser::RuleExpression;
}


std::any gscParser::ExpressionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression(this);
  else
    return visitor->visitChildren(this);
}

gscParser::ExpressionContext* gscParser::expression() {
  ExpressionContext *_localctx = _tracker.createInstance<ExpressionContext>(_ctx, getState());
  enterRule(_localctx, 58, gscParser::RuleExpression);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(459);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 41, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(451);
      set_expression();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(452);
      expression0(0);
      setState(453);
      match(gscParser::T__50);
      setState(454);
      expression();
      setState(455);
      match(gscParser::T__16);
      setState(456);
      expression();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(458);
      expression0(0);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Set_expressionContext ------------------------------------------------------------------

gscParser::Set_expressionContext::Set_expressionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Left_valueContext* gscParser::Set_expressionContext::left_value() {
  return getRuleContext<gscParser::Left_valueContext>(0);
}

gscParser::ExpressionContext* gscParser::Set_expressionContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Set_expressionContext::getRuleIndex() const {
  return gscParser::RuleSet_expression;
}


std::any gscParser::Set_expressionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitSet_expression(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Set_expressionContext* gscParser::set_expression() {
  Set_expressionContext *_localctx = _tracker.createInstance<Set_expressionContext>(_ctx, getState());
  enterRule(_localctx, 60, gscParser::RuleSet_expression);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(461);
    left_value(0);
    setState(462);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 9218868437227438080) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(463);
    expression();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Expression0Context ------------------------------------------------------------------

gscParser::Expression0Context::Expression0Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression1Context* gscParser::Expression0Context::expression1() {
  return getRuleContext<gscParser::Expression1Context>(0);
}

gscParser::Expression0Context* gscParser::Expression0Context::expression0() {
  return getRuleContext<gscParser::Expression0Context>(0);
}


size_t gscParser::Expression0Context::getRuleIndex() const {
  return gscParser::RuleExpression0;
}


std::any gscParser::Expression0Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression0(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression0Context* gscParser::expression0() {
   return expression0(0);
}

gscParser::Expression0Context* gscParser::expression0(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression0Context *_localctx = _tracker.createInstance<Expression0Context>(_ctx, parentState);
  gscParser::Expression0Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 62;
  enterRecursionRule(_localctx, 62, gscParser::RuleExpression0, precedence);

    

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(466);
    expression1(0);
    _ctx->stop = _input->LT(-1);
    setState(473);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 42, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression0Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression0);
        setState(468);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(469);
        match(gscParser::T__62);
        setState(470);
        expression1(0); 
      }
      setState(475);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 42, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression1Context ------------------------------------------------------------------

gscParser::Expression1Context::Expression1Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression2Context* gscParser::Expression1Context::expression2() {
  return getRuleContext<gscParser::Expression2Context>(0);
}

gscParser::Expression1Context* gscParser::Expression1Context::expression1() {
  return getRuleContext<gscParser::Expression1Context>(0);
}


size_t gscParser::Expression1Context::getRuleIndex() const {
  return gscParser::RuleExpression1;
}


std::any gscParser::Expression1Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression1(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression1Context* gscParser::expression1() {
   return expression1(0);
}

gscParser::Expression1Context* gscParser::expression1(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression1Context *_localctx = _tracker.createInstance<Expression1Context>(_ctx, parentState);
  gscParser::Expression1Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 64;
  enterRecursionRule(_localctx, 64, gscParser::RuleExpression1, precedence);

    

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(477);
    expression2(0);
    _ctx->stop = _input->LT(-1);
    setState(484);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression1Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression1);
        setState(479);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(480);
        match(gscParser::T__63);
        setState(481);
        expression2(0); 
      }
      setState(486);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression2Context ------------------------------------------------------------------

gscParser::Expression2Context::Expression2Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression3Context* gscParser::Expression2Context::expression3() {
  return getRuleContext<gscParser::Expression3Context>(0);
}

gscParser::Expression2Context* gscParser::Expression2Context::expression2() {
  return getRuleContext<gscParser::Expression2Context>(0);
}


size_t gscParser::Expression2Context::getRuleIndex() const {
  return gscParser::RuleExpression2;
}


std::any gscParser::Expression2Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression2(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression2Context* gscParser::expression2() {
   return expression2(0);
}

gscParser::Expression2Context* gscParser::expression2(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression2Context *_localctx = _tracker.createInstance<Expression2Context>(_ctx, parentState);
  gscParser::Expression2Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 66;
  enterRecursionRule(_localctx, 66, gscParser::RuleExpression2, precedence);

    

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(488);
    expression3(0);
    _ctx->stop = _input->LT(-1);
    setState(495);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 44, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression2Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression2);
        setState(490);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(491);
        match(gscParser::T__64);
        setState(492);
        expression3(0); 
      }
      setState(497);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 44, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression3Context ------------------------------------------------------------------

gscParser::Expression3Context::Expression3Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression4Context* gscParser::Expression3Context::expression4() {
  return getRuleContext<gscParser::Expression4Context>(0);
}

gscParser::Expression3Context* gscParser::Expression3Context::expression3() {
  return getRuleContext<gscParser::Expression3Context>(0);
}


size_t gscParser::Expression3Context::getRuleIndex() const {
  return gscParser::RuleExpression3;
}


std::any gscParser::Expression3Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression3(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression3Context* gscParser::expression3() {
   return expression3(0);
}

gscParser::Expression3Context* gscParser::expression3(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression3Context *_localctx = _tracker.createInstance<Expression3Context>(_ctx, parentState);
  gscParser::Expression3Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 68;
  enterRecursionRule(_localctx, 68, gscParser::RuleExpression3, precedence);

    

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(499);
    expression4(0);
    _ctx->stop = _input->LT(-1);
    setState(506);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 45, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression3Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression3);
        setState(501);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(502);
        match(gscParser::T__65);
        setState(503);
        expression4(0); 
      }
      setState(508);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 45, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression4Context ------------------------------------------------------------------

gscParser::Expression4Context::Expression4Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression5Context* gscParser::Expression4Context::expression5() {
  return getRuleContext<gscParser::Expression5Context>(0);
}

gscParser::Expression4Context* gscParser::Expression4Context::expression4() {
  return getRuleContext<gscParser::Expression4Context>(0);
}


size_t gscParser::Expression4Context::getRuleIndex() const {
  return gscParser::RuleExpression4;
}


std::any gscParser::Expression4Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression4(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression4Context* gscParser::expression4() {
   return expression4(0);
}

gscParser::Expression4Context* gscParser::expression4(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression4Context *_localctx = _tracker.createInstance<Expression4Context>(_ctx, parentState);
  gscParser::Expression4Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 70;
  enterRecursionRule(_localctx, 70, gscParser::RuleExpression4, precedence);

    

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(510);
    expression5(0);
    _ctx->stop = _input->LT(-1);
    setState(517);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 46, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression4Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression4);
        setState(512);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(513);
        match(gscParser::T__66);
        setState(514);
        expression5(0); 
      }
      setState(519);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 46, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression5Context ------------------------------------------------------------------

gscParser::Expression5Context::Expression5Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression6Context* gscParser::Expression5Context::expression6() {
  return getRuleContext<gscParser::Expression6Context>(0);
}

gscParser::Expression5Context* gscParser::Expression5Context::expression5() {
  return getRuleContext<gscParser::Expression5Context>(0);
}


size_t gscParser::Expression5Context::getRuleIndex() const {
  return gscParser::RuleExpression5;
}


std::any gscParser::Expression5Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression5(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression5Context* gscParser::expression5() {
   return expression5(0);
}

gscParser::Expression5Context* gscParser::expression5(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression5Context *_localctx = _tracker.createInstance<Expression5Context>(_ctx, parentState);
  gscParser::Expression5Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 72;
  enterRecursionRule(_localctx, 72, gscParser::RuleExpression5, precedence);

    

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(521);
    expression6(0);
    _ctx->stop = _input->LT(-1);
    setState(528);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 47, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression5Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression5);
        setState(523);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(524);
        match(gscParser::T__32);
        setState(525);
        expression6(0); 
      }
      setState(530);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 47, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression6Context ------------------------------------------------------------------

gscParser::Expression6Context::Expression6Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression7Context* gscParser::Expression6Context::expression7() {
  return getRuleContext<gscParser::Expression7Context>(0);
}

gscParser::Expression6Context* gscParser::Expression6Context::expression6() {
  return getRuleContext<gscParser::Expression6Context>(0);
}


size_t gscParser::Expression6Context::getRuleIndex() const {
  return gscParser::RuleExpression6;
}


std::any gscParser::Expression6Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression6(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression6Context* gscParser::expression6() {
   return expression6(0);
}

gscParser::Expression6Context* gscParser::expression6(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression6Context *_localctx = _tracker.createInstance<Expression6Context>(_ctx, parentState);
  gscParser::Expression6Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 74;
  enterRecursionRule(_localctx, 74, gscParser::RuleExpression6, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(532);
    expression7(0);
    _ctx->stop = _input->LT(-1);
    setState(539);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 48, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression6Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression6);
        setState(534);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(535);
        _la = _input->LA(1);
        if (!(((((_la - 68) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 68)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(536);
        expression7(0); 
      }
      setState(541);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 48, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression7Context ------------------------------------------------------------------

gscParser::Expression7Context::Expression7Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression8Context* gscParser::Expression7Context::expression8() {
  return getRuleContext<gscParser::Expression8Context>(0);
}

gscParser::Expression7Context* gscParser::Expression7Context::expression7() {
  return getRuleContext<gscParser::Expression7Context>(0);
}


size_t gscParser::Expression7Context::getRuleIndex() const {
  return gscParser::RuleExpression7;
}


std::any gscParser::Expression7Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression7(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression7Context* gscParser::expression7() {
   return expression7(0);
}

gscParser::Expression7Context* gscParser::expression7(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression7Context *_localctx = _tracker.createInstance<Expression7Context>(_ctx, parentState);
  gscParser::Expression7Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 76;
  enterRecursionRule(_localctx, 76, gscParser::RuleExpression7, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(543);
    expression8(0);
    _ctx->stop = _input->LT(-1);
    setState(550);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 49, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression7Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression7);
        setState(545);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(546);
        _la = _input->LA(1);
        if (!(((((_la - 29) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 29)) & 131941395333123) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(547);
        expression8(0); 
      }
      setState(552);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 49, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression8Context ------------------------------------------------------------------

gscParser::Expression8Context::Expression8Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression9Context* gscParser::Expression8Context::expression9() {
  return getRuleContext<gscParser::Expression9Context>(0);
}

gscParser::Expression8Context* gscParser::Expression8Context::expression8() {
  return getRuleContext<gscParser::Expression8Context>(0);
}


size_t gscParser::Expression8Context::getRuleIndex() const {
  return gscParser::RuleExpression8;
}


std::any gscParser::Expression8Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression8(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression8Context* gscParser::expression8() {
   return expression8(0);
}

gscParser::Expression8Context* gscParser::expression8(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression8Context *_localctx = _tracker.createInstance<Expression8Context>(_ctx, parentState);
  gscParser::Expression8Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 78;
  enterRecursionRule(_localctx, 78, gscParser::RuleExpression8, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(554);
    expression9(0);
    _ctx->stop = _input->LT(-1);
    setState(561);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 50, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression8Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression8);
        setState(556);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(557);
        _la = _input->LA(1);
        if (!(_la == gscParser::T__28

        || _la == gscParser::T__71)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(558);
        expression9(0); 
      }
      setState(563);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 50, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression9Context ------------------------------------------------------------------

gscParser::Expression9Context::Expression9Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression10Context* gscParser::Expression9Context::expression10() {
  return getRuleContext<gscParser::Expression10Context>(0);
}

gscParser::Expression9Context* gscParser::Expression9Context::expression9() {
  return getRuleContext<gscParser::Expression9Context>(0);
}


size_t gscParser::Expression9Context::getRuleIndex() const {
  return gscParser::RuleExpression9;
}


std::any gscParser::Expression9Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression9(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression9Context* gscParser::expression9() {
   return expression9(0);
}

gscParser::Expression9Context* gscParser::expression9(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression9Context *_localctx = _tracker.createInstance<Expression9Context>(_ctx, parentState);
  gscParser::Expression9Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 80;
  enterRecursionRule(_localctx, 80, gscParser::RuleExpression9, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(565);
    expression10(0);
    _ctx->stop = _input->LT(-1);
    setState(572);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 51, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression9Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression9);
        setState(567);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(568);
        _la = _input->LA(1);
        if (!(_la == gscParser::T__75

        || _la == gscParser::T__76)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(569);
        expression10(0); 
      }
      setState(574);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 51, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression10Context ------------------------------------------------------------------

gscParser::Expression10Context::Expression10Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression11Context* gscParser::Expression10Context::expression11() {
  return getRuleContext<gscParser::Expression11Context>(0);
}

gscParser::Expression10Context* gscParser::Expression10Context::expression10() {
  return getRuleContext<gscParser::Expression10Context>(0);
}


size_t gscParser::Expression10Context::getRuleIndex() const {
  return gscParser::RuleExpression10;
}


std::any gscParser::Expression10Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression10(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression10Context* gscParser::expression10() {
   return expression10(0);
}

gscParser::Expression10Context* gscParser::expression10(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression10Context *_localctx = _tracker.createInstance<Expression10Context>(_ctx, parentState);
  gscParser::Expression10Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 82;
  enterRecursionRule(_localctx, 82, gscParser::RuleExpression10, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(576);
    expression11(0);
    _ctx->stop = _input->LT(-1);
    setState(583);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 52, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression10Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression10);
        setState(578);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(579);
        _la = _input->LA(1);
        if (!(_la == gscParser::T__77

        || _la == gscParser::T__78)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(580);
        expression11(0); 
      }
      setState(585);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 52, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression11Context ------------------------------------------------------------------

gscParser::Expression11Context::Expression11Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression12Context* gscParser::Expression11Context::expression12() {
  return getRuleContext<gscParser::Expression12Context>(0);
}

gscParser::Expression11Context* gscParser::Expression11Context::expression11() {
  return getRuleContext<gscParser::Expression11Context>(0);
}


size_t gscParser::Expression11Context::getRuleIndex() const {
  return gscParser::RuleExpression11;
}


std::any gscParser::Expression11Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression11(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Expression11Context* gscParser::expression11() {
   return expression11(0);
}

gscParser::Expression11Context* gscParser::expression11(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Expression11Context *_localctx = _tracker.createInstance<Expression11Context>(_ctx, parentState);
  gscParser::Expression11Context *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 84;
  enterRecursionRule(_localctx, 84, gscParser::RuleExpression11, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(587);
    expression12();
    _ctx->stop = _input->LT(-1);
    setState(594);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 53, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Expression11Context>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpression11);
        setState(589);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(590);
        _la = _input->LA(1);
        if (!(((((_la - 32) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 32)) & 844424930131969) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(591);
        expression12(); 
      }
      setState(596);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 53, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Expression12Context ------------------------------------------------------------------

gscParser::Expression12Context::Expression12Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression13Context* gscParser::Expression12Context::expression13() {
  return getRuleContext<gscParser::Expression13Context>(0);
}

gscParser::Left_valueContext* gscParser::Expression12Context::left_value() {
  return getRuleContext<gscParser::Left_valueContext>(0);
}

gscParser::Is_expressionContext* gscParser::Expression12Context::is_expression() {
  return getRuleContext<gscParser::Is_expressionContext>(0);
}


size_t gscParser::Expression12Context::getRuleIndex() const {
  return gscParser::RuleExpression12;
}


std::any gscParser::Expression12Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression12(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Expression12Context* gscParser::expression12() {
  Expression12Context *_localctx = _tracker.createInstance<Expression12Context>(_ctx, getState());
  enterRule(_localctx, 86, gscParser::RuleExpression12);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(606);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 54, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(597);
      _la = _input->LA(1);
      if (!(_la == gscParser::T__81

      || _la == gscParser::T__82)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(598);
      expression13();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(599);
      _la = _input->LA(1);
      if (!(_la == gscParser::T__83

      || _la == gscParser::T__84)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(600);
      left_value(0);
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(601);
      left_value(0);
      setState(602);
      _la = _input->LA(1);
      if (!(_la == gscParser::T__83

      || _la == gscParser::T__84)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(604);
      is_expression();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(605);
      expression13();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Expression13Context ------------------------------------------------------------------

gscParser::Expression13Context::Expression13Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Function_call_expContext* gscParser::Expression13Context::function_call_exp() {
  return getRuleContext<gscParser::Function_call_expContext>(0);
}

gscParser::Expression14Context* gscParser::Expression13Context::expression14() {
  return getRuleContext<gscParser::Expression14Context>(0);
}


size_t gscParser::Expression13Context::getRuleIndex() const {
  return gscParser::RuleExpression13;
}


std::any gscParser::Expression13Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression13(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Expression13Context* gscParser::expression13() {
  Expression13Context *_localctx = _tracker.createInstance<Expression13Context>(_ctx, getState());
  enterRule(_localctx, 88, gscParser::RuleExpression13);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(610);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 55, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(608);
      function_call_exp();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(609);
      expression14();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Expression14Context ------------------------------------------------------------------

gscParser::Expression14Context::Expression14Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Const_exprContext* gscParser::Expression14Context::const_expr() {
  return getRuleContext<gscParser::Const_exprContext>(0);
}

gscParser::Expression15Context* gscParser::Expression14Context::expression15() {
  return getRuleContext<gscParser::Expression15Context>(0);
}

gscParser::Left_valueContext* gscParser::Expression14Context::left_value() {
  return getRuleContext<gscParser::Left_valueContext>(0);
}


size_t gscParser::Expression14Context::getRuleIndex() const {
  return gscParser::RuleExpression14;
}


std::any gscParser::Expression14Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression14(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Expression14Context* gscParser::expression14() {
  Expression14Context *_localctx = _tracker.createInstance<Expression14Context>(_ctx, getState());
  enterRule(_localctx, 90, gscParser::RuleExpression14);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(615);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 56, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(612);
      const_expr();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(613);
      expression15();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(614);
      left_value(0);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Expression15Context ------------------------------------------------------------------

gscParser::Expression15Context::Expression15Context(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::ExpressionContext* gscParser::Expression15Context::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}


size_t gscParser::Expression15Context::getRuleIndex() const {
  return gscParser::RuleExpression15;
}


std::any gscParser::Expression15Context::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitExpression15(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Expression15Context* gscParser::expression15() {
  Expression15Context *_localctx = _tracker.createInstance<Expression15Context>(_ctx, getState());
  enterRule(_localctx, 92, gscParser::RuleExpression15);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(617);
    match(gscParser::T__6);
    setState(618);
    expression();
    setState(619);
    match(gscParser::T__7);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Is_expressionContext ------------------------------------------------------------------

gscParser::Is_expressionContext::Is_expressionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Expression13Context* gscParser::Is_expressionContext::expression13() {
  return getRuleContext<gscParser::Expression13Context>(0);
}

tree::TerminalNode* gscParser::Is_expressionContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

tree::TerminalNode* gscParser::Is_expressionContext::BOOL_VALUE() {
  return getToken(gscParser::BOOL_VALUE, 0);
}

tree::TerminalNode* gscParser::Is_expressionContext::UNDEFINED_VALUE() {
  return getToken(gscParser::UNDEFINED_VALUE, 0);
}


size_t gscParser::Is_expressionContext::getRuleIndex() const {
  return gscParser::RuleIs_expression;
}


std::any gscParser::Is_expressionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitIs_expression(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Is_expressionContext* gscParser::is_expression() {
  Is_expressionContext *_localctx = _tracker.createInstance<Is_expressionContext>(_ctx, getState());
  enterRule(_localctx, 94, gscParser::RuleIs_expression);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(621);
    expression13();
    setState(622);
    match(gscParser::T__85);
    setState(624);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == gscParser::T__86) {
      setState(623);
      match(gscParser::T__86);
    }
    setState(626);
    _la = _input->LA(1);
    if (!(_la == gscParser::T__20 || ((((_la - 104) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 104)) & 7) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Function_call_expContext ------------------------------------------------------------------

gscParser::Function_call_expContext::Function_call_expContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Function_callContext* gscParser::Function_call_expContext::function_call() {
  return getRuleContext<gscParser::Function_callContext>(0);
}

gscParser::Expression14Context* gscParser::Function_call_expContext::expression14() {
  return getRuleContext<gscParser::Expression14Context>(0);
}

gscParser::Function_componentContext* gscParser::Function_call_expContext::function_component() {
  return getRuleContext<gscParser::Function_componentContext>(0);
}

gscParser::Expression_listContext* gscParser::Function_call_expContext::expression_list() {
  return getRuleContext<gscParser::Expression_listContext>(0);
}


size_t gscParser::Function_call_expContext::getRuleIndex() const {
  return gscParser::RuleFunction_call_exp;
}


std::any gscParser::Function_call_expContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitFunction_call_exp(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Function_call_expContext* gscParser::function_call_exp() {
  Function_call_expContext *_localctx = _tracker.createInstance<Function_call_expContext>(_ctx, getState());
  enterRule(_localctx, 96, gscParser::RuleFunction_call_exp);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(638);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 59, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(628);
      function_call(0);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(629);
      expression14();
      setState(631);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 88) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 88)) & 15) != 0)) {
        setState(630);
        _la = _input->LA(1);
        if (!(((((_la - 88) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 88)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(633);
      function_component();
      setState(634);
      match(gscParser::T__6);
      setState(635);
      expression_list();
      setState(636);
      match(gscParser::T__7);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Function_callContext ------------------------------------------------------------------

gscParser::Function_callContext::Function_callContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Function_componentContext* gscParser::Function_callContext::function_component() {
  return getRuleContext<gscParser::Function_componentContext>(0);
}

gscParser::Expression_listContext* gscParser::Function_callContext::expression_list() {
  return getRuleContext<gscParser::Expression_listContext>(0);
}

gscParser::Const_exprContext* gscParser::Function_callContext::const_expr() {
  return getRuleContext<gscParser::Const_exprContext>(0);
}

gscParser::Expression15Context* gscParser::Function_callContext::expression15() {
  return getRuleContext<gscParser::Expression15Context>(0);
}

gscParser::Function_callContext* gscParser::Function_callContext::function_call() {
  return getRuleContext<gscParser::Function_callContext>(0);
}


size_t gscParser::Function_callContext::getRuleIndex() const {
  return gscParser::RuleFunction_call;
}


std::any gscParser::Function_callContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitFunction_call(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Function_callContext* gscParser::function_call() {
   return function_call(0);
}

gscParser::Function_callContext* gscParser::function_call(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Function_callContext *_localctx = _tracker.createInstance<Function_callContext>(_ctx, parentState);
  gscParser::Function_callContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 98;
  enterRecursionRule(_localctx, 98, gscParser::RuleFunction_call, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(661);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 63, _ctx)) {
    case 1: {
      setState(642);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 88) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 88)) & 15) != 0)) {
        setState(641);
        _la = _input->LA(1);
        if (!(((((_la - 88) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 88)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(644);
      function_component();
      setState(645);
      match(gscParser::T__6);
      setState(646);
      expression_list();
      setState(647);
      match(gscParser::T__7);
      break;
    }

    case 2: {
      setState(651);
      _errHandler->sync(this);
      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 61, _ctx)) {
      case 1: {
        setState(649);
        const_expr();
        break;
      }

      case 2: {
        setState(650);
        expression15();
        break;
      }

      default:
        break;
      }
      setState(654);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 88) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 88)) & 15) != 0)) {
        setState(653);
        _la = _input->LA(1);
        if (!(((((_la - 88) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 88)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(656);
      function_component();
      setState(657);
      match(gscParser::T__6);
      setState(658);
      expression_list();
      setState(659);
      match(gscParser::T__7);
      break;
    }

    default:
      break;
    }
    _ctx->stop = _input->LT(-1);
    setState(674);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 65, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<Function_callContext>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleFunction_call);
        setState(663);

        if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
        setState(665);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (((((_la - 88) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 88)) & 15) != 0)) {
          setState(664);
          _la = _input->LA(1);
          if (!(((((_la - 88) & ~ 0x3fULL) == 0) &&
            ((1ULL << (_la - 88)) & 15) != 0))) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
        }
        setState(667);
        function_component();
        setState(668);
        match(gscParser::T__6);
        setState(669);
        expression_list();
        setState(670);
        match(gscParser::T__7); 
      }
      setState(676);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 65, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Left_valueContext ------------------------------------------------------------------

gscParser::Left_valueContext::Left_valueContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::IdfContext* gscParser::Left_valueContext::idf() {
  return getRuleContext<gscParser::IdfContext>(0);
}

gscParser::Function_callContext* gscParser::Left_valueContext::function_call() {
  return getRuleContext<gscParser::Function_callContext>(0);
}

gscParser::Const_exprContext* gscParser::Left_valueContext::const_expr() {
  return getRuleContext<gscParser::Const_exprContext>(0);
}

gscParser::Expression15Context* gscParser::Left_valueContext::expression15() {
  return getRuleContext<gscParser::Expression15Context>(0);
}

gscParser::ExpressionContext* gscParser::Left_valueContext::expression() {
  return getRuleContext<gscParser::ExpressionContext>(0);
}

gscParser::Left_valueContext* gscParser::Left_valueContext::left_value() {
  return getRuleContext<gscParser::Left_valueContext>(0);
}


size_t gscParser::Left_valueContext::getRuleIndex() const {
  return gscParser::RuleLeft_value;
}


std::any gscParser::Left_valueContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitLeft_value(this);
  else
    return visitor->visitChildren(this);
}


gscParser::Left_valueContext* gscParser::left_value() {
   return left_value(0);
}

gscParser::Left_valueContext* gscParser::left_value(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  gscParser::Left_valueContext *_localctx = _tracker.createInstance<Left_valueContext>(_ctx, parentState);
  gscParser::Left_valueContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 100;
  enterRecursionRule(_localctx, 100, gscParser::RuleLeft_value, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(701);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 69, _ctx)) {
    case 1: {
      setState(678);
      idf();
      break;
    }

    case 2: {
      setState(682);
      _errHandler->sync(this);
      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 66, _ctx)) {
      case 1: {
        setState(679);
        function_call(0);
        break;
      }

      case 2: {
        setState(680);
        const_expr();
        break;
      }

      case 3: {
        setState(681);
        expression15();
        break;
      }

      default:
        break;
      }
      setState(684);
      _la = _input->LA(1);
      if (!(_la == gscParser::T__91

      || _la == gscParser::T__92)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(690);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case gscParser::IDENTIFIER: {
          setState(685);
          idf();
          break;
        }

        case gscParser::T__6: {
          setState(686);
          match(gscParser::T__6);
          setState(687);
          expression();
          setState(688);
          match(gscParser::T__7);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      break;
    }

    case 3: {
      setState(695);
      _errHandler->sync(this);
      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 68, _ctx)) {
      case 1: {
        setState(692);
        function_call(0);
        break;
      }

      case 2: {
        setState(693);
        const_expr();
        break;
      }

      case 3: {
        setState(694);
        expression15();
        break;
      }

      default:
        break;
      }
      setState(697);
      match(gscParser::T__24);
      setState(698);
      expression();
      setState(699);
      match(gscParser::T__25);
      break;
    }

    default:
      break;
    }
    _ctx->stop = _input->LT(-1);
    setState(719);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 72, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(717);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 71, _ctx)) {
        case 1: {
          _localctx = _tracker.createInstance<Left_valueContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleLeft_value);
          setState(703);

          if (!(precpred(_ctx, 4))) throw FailedPredicateException(this, "precpred(_ctx, 4)");
          setState(704);
          _la = _input->LA(1);
          if (!(_la == gscParser::T__91

          || _la == gscParser::T__92)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(710);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case gscParser::IDENTIFIER: {
              setState(705);
              idf();
              break;
            }

            case gscParser::T__6: {
              setState(706);
              match(gscParser::T__6);
              setState(707);
              expression();
              setState(708);
              match(gscParser::T__7);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          break;
        }

        case 2: {
          _localctx = _tracker.createInstance<Left_valueContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleLeft_value);
          setState(712);

          if (!(precpred(_ctx, 3))) throw FailedPredicateException(this, "precpred(_ctx, 3)");
          setState(713);
          match(gscParser::T__24);
          setState(714);
          expression();
          setState(715);
          match(gscParser::T__25);
          break;
        }

        default:
          break;
        } 
      }
      setState(721);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 72, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- Const_exprContext ------------------------------------------------------------------

gscParser::Const_exprContext::Const_exprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Class_initContext* gscParser::Const_exprContext::class_init() {
  return getRuleContext<gscParser::Class_initContext>(0);
}

gscParser::Array_defContext* gscParser::Const_exprContext::array_def() {
  return getRuleContext<gscParser::Array_defContext>(0);
}

gscParser::Struct_defContext* gscParser::Const_exprContext::struct_def() {
  return getRuleContext<gscParser::Struct_defContext>(0);
}

gscParser::FunctionContext* gscParser::Const_exprContext::function() {
  return getRuleContext<gscParser::FunctionContext>(0);
}

gscParser::Const_expr_staticContext* gscParser::Const_exprContext::const_expr_static() {
  return getRuleContext<gscParser::Const_expr_staticContext>(0);
}

tree::TerminalNode* gscParser::Const_exprContext::UNDEFINED_VALUE() {
  return getToken(gscParser::UNDEFINED_VALUE, 0);
}

gscParser::Function_refContext* gscParser::Const_exprContext::function_ref() {
  return getRuleContext<gscParser::Function_refContext>(0);
}

gscParser::Anim_refContext* gscParser::Const_exprContext::anim_ref() {
  return getRuleContext<gscParser::Anim_refContext>(0);
}

gscParser::Data_refContext* gscParser::Const_exprContext::data_ref() {
  return getRuleContext<gscParser::Data_refContext>(0);
}


size_t gscParser::Const_exprContext::getRuleIndex() const {
  return gscParser::RuleConst_expr;
}


std::any gscParser::Const_exprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitConst_expr(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Const_exprContext* gscParser::const_expr() {
  Const_exprContext *_localctx = _tracker.createInstance<Const_exprContext>(_ctx, getState());
  enterRule(_localctx, 102, gscParser::RuleConst_expr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(731);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 73, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(722);
      class_init();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(723);
      array_def();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(724);
      struct_def();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(725);
      function();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(726);
      const_expr_static();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(727);
      match(gscParser::UNDEFINED_VALUE);
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(728);
      function_ref();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(729);
      anim_ref();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(730);
      data_ref();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Const_expr_staticContext ------------------------------------------------------------------

gscParser::Const_expr_staticContext::Const_expr_staticContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

gscParser::Vector_valueContext* gscParser::Const_expr_staticContext::vector_value() {
  return getRuleContext<gscParser::Vector_valueContext>(0);
}

gscParser::NumberContext* gscParser::Const_expr_staticContext::number() {
  return getRuleContext<gscParser::NumberContext>(0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::BOOL_VALUE() {
  return getToken(gscParser::BOOL_VALUE, 0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::FLOATVAL() {
  return getToken(gscParser::FLOATVAL, 0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::STRING() {
  return getToken(gscParser::STRING, 0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::ISTRING() {
  return getToken(gscParser::ISTRING, 0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::HASHSTRING() {
  return getToken(gscParser::HASHSTRING, 0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::SCR_HASH() {
  return getToken(gscParser::SCR_HASH, 0);
}

tree::TerminalNode* gscParser::Const_expr_staticContext::ANIMTREE_IDENTIFIER() {
  return getToken(gscParser::ANIMTREE_IDENTIFIER, 0);
}


size_t gscParser::Const_expr_staticContext::getRuleIndex() const {
  return gscParser::RuleConst_expr_static;
}


std::any gscParser::Const_expr_staticContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitConst_expr_static(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Const_expr_staticContext* gscParser::const_expr_static() {
  Const_expr_staticContext *_localctx = _tracker.createInstance<Const_expr_staticContext>(_ctx, getState());
  enterRule(_localctx, 104, gscParser::RuleConst_expr_static);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(742);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case gscParser::T__6: {
        enterOuterAlt(_localctx, 1);
        setState(733);
        vector_value();
        break;
      }

      case gscParser::INTEGER10:
      case gscParser::INTEGER16:
      case gscParser::INTEGER8:
      case gscParser::INTEGER2:
      case gscParser::IDENTIFIER: {
        enterOuterAlt(_localctx, 2);
        setState(734);
        number();
        break;
      }

      case gscParser::BOOL_VALUE: {
        enterOuterAlt(_localctx, 3);
        setState(735);
        match(gscParser::BOOL_VALUE);
        break;
      }

      case gscParser::FLOATVAL: {
        enterOuterAlt(_localctx, 4);
        setState(736);
        match(gscParser::FLOATVAL);
        break;
      }

      case gscParser::STRING: {
        enterOuterAlt(_localctx, 5);
        setState(737);
        match(gscParser::STRING);
        break;
      }

      case gscParser::ISTRING: {
        enterOuterAlt(_localctx, 6);
        setState(738);
        match(gscParser::ISTRING);
        break;
      }

      case gscParser::HASHSTRING: {
        enterOuterAlt(_localctx, 7);
        setState(739);
        match(gscParser::HASHSTRING);
        break;
      }

      case gscParser::SCR_HASH: {
        enterOuterAlt(_localctx, 8);
        setState(740);
        match(gscParser::SCR_HASH);
        break;
      }

      case gscParser::ANIMTREE_IDENTIFIER: {
        enterOuterAlt(_localctx, 9);
        setState(741);
        match(gscParser::ANIMTREE_IDENTIFIER);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Function_refContext ------------------------------------------------------------------

gscParser::Function_refContext::Function_refContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Function_refContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Function_refContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}

tree::TerminalNode* gscParser::Function_refContext::PATH() {
  return getToken(gscParser::PATH, 0);
}

gscParser::Left_valueContext* gscParser::Function_refContext::left_value() {
  return getRuleContext<gscParser::Left_valueContext>(0);
}


size_t gscParser::Function_refContext::getRuleIndex() const {
  return gscParser::RuleFunction_ref;
}


std::any gscParser::Function_refContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitFunction_ref(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Function_refContext* gscParser::function_ref() {
  Function_refContext *_localctx = _tracker.createInstance<Function_refContext>(_ctx, getState());
  enterRule(_localctx, 106, gscParser::RuleFunction_ref);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(759);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 76, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(744);
      match(gscParser::T__32);
      setState(747);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 75, _ctx)) {
      case 1: {
        setState(745);
        match(gscParser::IDENTIFIER);
        setState(746);
        match(gscParser::T__30);
        break;
      }

      default:
        break;
      }
      setState(749);
      match(gscParser::IDENTIFIER);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(750);
      match(gscParser::T__93);
      setState(751);
      match(gscParser::IDENTIFIER);
      setState(752);
      match(gscParser::T__28);
      setState(753);
      match(gscParser::PATH);
      setState(754);
      match(gscParser::T__29);
      setState(755);
      match(gscParser::T__30);
      setState(756);
      match(gscParser::IDENTIFIER);
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(757);
      match(gscParser::T__32);
      setState(758);
      left_value(0);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Anim_refContext ------------------------------------------------------------------

gscParser::Anim_refContext::Anim_refContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Anim_refContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Anim_refContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}


size_t gscParser::Anim_refContext::getRuleIndex() const {
  return gscParser::RuleAnim_ref;
}


std::any gscParser::Anim_refContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitAnim_ref(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Anim_refContext* gscParser::anim_ref() {
  Anim_refContext *_localctx = _tracker.createInstance<Anim_refContext>(_ctx, getState());
  enterRule(_localctx, 108, gscParser::RuleAnim_ref);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(761);
    match(gscParser::T__80);
    setState(762);
    match(gscParser::IDENTIFIER);
    setState(765);
    _errHandler->sync(this);

    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 77, _ctx)) {
    case 1: {
      setState(763);
      match(gscParser::T__30);
      setState(764);
      match(gscParser::IDENTIFIER);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Data_refContext ------------------------------------------------------------------

gscParser::Data_refContext::Data_refContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> gscParser::Data_refContext::IDENTIFIER() {
  return getTokens(gscParser::IDENTIFIER);
}

tree::TerminalNode* gscParser::Data_refContext::IDENTIFIER(size_t i) {
  return getToken(gscParser::IDENTIFIER, i);
}


size_t gscParser::Data_refContext::getRuleIndex() const {
  return gscParser::RuleData_ref;
}


std::any gscParser::Data_refContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitData_ref(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Data_refContext* gscParser::data_ref() {
  Data_refContext *_localctx = _tracker.createInstance<Data_refContext>(_ctx, getState());
  enterRule(_localctx, 110, gscParser::RuleData_ref);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(767);
    match(gscParser::T__93);
    setState(768);
    match(gscParser::T__24);
    setState(769);
    match(gscParser::IDENTIFIER);
    setState(770);
    match(gscParser::T__9);
    setState(771);
    match(gscParser::IDENTIFIER);
    setState(772);
    match(gscParser::T__25);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- NumberContext ------------------------------------------------------------------

gscParser::NumberContext::NumberContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::NumberContext::INTEGER10() {
  return getToken(gscParser::INTEGER10, 0);
}

tree::TerminalNode* gscParser::NumberContext::INTEGER16() {
  return getToken(gscParser::INTEGER16, 0);
}

tree::TerminalNode* gscParser::NumberContext::INTEGER8() {
  return getToken(gscParser::INTEGER8, 0);
}

tree::TerminalNode* gscParser::NumberContext::INTEGER2() {
  return getToken(gscParser::INTEGER2, 0);
}

tree::TerminalNode* gscParser::NumberContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}


size_t gscParser::NumberContext::getRuleIndex() const {
  return gscParser::RuleNumber;
}


std::any gscParser::NumberContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitNumber(this);
  else
    return visitor->visitChildren(this);
}

gscParser::NumberContext* gscParser::number() {
  NumberContext *_localctx = _tracker.createInstance<NumberContext>(_ctx, getState());
  enterRule(_localctx, 112, gscParser::RuleNumber);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(774);
    _la = _input->LA(1);
    if (!(((((_la - 98) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 98)) & 271) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Vector_valueContext ------------------------------------------------------------------

gscParser::Vector_valueContext::Vector_valueContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::ExpressionContext *> gscParser::Vector_valueContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::Vector_valueContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::Vector_valueContext::getRuleIndex() const {
  return gscParser::RuleVector_value;
}


std::any gscParser::Vector_valueContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitVector_value(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Vector_valueContext* gscParser::vector_value() {
  Vector_valueContext *_localctx = _tracker.createInstance<Vector_valueContext>(_ctx, getState());
  enterRule(_localctx, 114, gscParser::RuleVector_value);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(776);
    match(gscParser::T__6);
    setState(777);
    expression();
    setState(778);
    match(gscParser::T__9);
    setState(779);
    expression();
    setState(780);
    match(gscParser::T__9);
    setState(781);
    expression();
    setState(782);
    match(gscParser::T__7);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Array_defContext ------------------------------------------------------------------

gscParser::Array_defContext::Array_defContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::ExpressionContext *> gscParser::Array_defContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::Array_defContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::Array_defContext::getRuleIndex() const {
  return gscParser::RuleArray_def;
}


std::any gscParser::Array_defContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitArray_def(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Array_defContext* gscParser::array_def() {
  Array_defContext *_localctx = _tracker.createInstance<Array_defContext>(_ctx, getState());
  enterRule(_localctx, 116, gscParser::RuleArray_def);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(784);
    match(gscParser::T__24);
    setState(806);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(788);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 78, _ctx)) {
      case 1: {
        setState(785);
        expression();
        setState(786);
        match(gscParser::T__16);
        break;
      }

      default:
        break;
      }
      setState(790);
      expression();
      setState(800);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 80, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(791);
          match(gscParser::T__9);
          setState(795);
          _errHandler->sync(this);

          switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 79, _ctx)) {
          case 1: {
            setState(792);
            expression();
            setState(793);
            match(gscParser::T__16);
            break;
          }

          default:
            break;
          }
          setState(797);
          expression(); 
        }
        setState(802);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 80, _ctx);
      }
      setState(804);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == gscParser::T__9) {
        setState(803);
        match(gscParser::T__9);
      }
    }
    setState(808);
    match(gscParser::T__25);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Struct_defContext ------------------------------------------------------------------

gscParser::Struct_defContext::Struct_defContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<gscParser::ExpressionContext *> gscParser::Struct_defContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::Struct_defContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::Struct_defContext::getRuleIndex() const {
  return gscParser::RuleStruct_def;
}


std::any gscParser::Struct_defContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitStruct_def(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Struct_defContext* gscParser::struct_def() {
  Struct_defContext *_localctx = _tracker.createInstance<Struct_defContext>(_ctx, getState());
  enterRule(_localctx, 118, gscParser::RuleStruct_def);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(810);
    match(gscParser::T__17);
    setState(827);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(811);
      expression();
      setState(812);
      match(gscParser::T__16);
      setState(813);
      expression();
      setState(821);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 83, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(814);
          match(gscParser::T__9);
          setState(815);
          expression();
          setState(816);
          match(gscParser::T__16);
          setState(817);
          expression(); 
        }
        setState(823);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 83, _ctx);
      }
      setState(825);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == gscParser::T__9) {
        setState(824);
        match(gscParser::T__9);
      }
    }
    setState(829);
    match(gscParser::T__18);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- Class_initContext ------------------------------------------------------------------

gscParser::Class_initContext::Class_initContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::Class_initContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}

std::vector<gscParser::ExpressionContext *> gscParser::Class_initContext::expression() {
  return getRuleContexts<gscParser::ExpressionContext>();
}

gscParser::ExpressionContext* gscParser::Class_initContext::expression(size_t i) {
  return getRuleContext<gscParser::ExpressionContext>(i);
}


size_t gscParser::Class_initContext::getRuleIndex() const {
  return gscParser::RuleClass_init;
}


std::any gscParser::Class_initContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitClass_init(this);
  else
    return visitor->visitChildren(this);
}

gscParser::Class_initContext* gscParser::class_init() {
  Class_initContext *_localctx = _tracker.createInstance<Class_initContext>(_ctx, getState());
  enterRule(_localctx, 120, gscParser::RuleClass_init);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(831);
    match(gscParser::T__94);
    setState(832);
    match(gscParser::IDENTIFIER);
    setState(833);
    match(gscParser::T__6);
    setState(842);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8923644032) != 0) || ((((_la - 81) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 81)) & 4022232991) != 0)) {
      setState(834);
      expression();
      setState(839);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == gscParser::T__9) {
        setState(835);
        match(gscParser::T__9);
        setState(836);
        expression();
        setState(841);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(844);
    match(gscParser::T__7);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- IdfContext ------------------------------------------------------------------

gscParser::IdfContext::IdfContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* gscParser::IdfContext::IDENTIFIER() {
  return getToken(gscParser::IDENTIFIER, 0);
}


size_t gscParser::IdfContext::getRuleIndex() const {
  return gscParser::RuleIdf;
}


std::any gscParser::IdfContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<gscVisitor*>(visitor))
    return parserVisitor->visitIdf(this);
  else
    return visitor->visitChildren(this);
}

gscParser::IdfContext* gscParser::idf() {
  IdfContext *_localctx = _tracker.createInstance<IdfContext>(_ctx, getState());
  enterRule(_localctx, 122, gscParser::RuleIdf);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(846);
    match(gscParser::IDENTIFIER);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

bool gscParser::sempred(RuleContext *context, size_t ruleIndex, size_t predicateIndex) {
  switch (ruleIndex) {
    case 31: return expression0Sempred(antlrcpp::downCast<Expression0Context *>(context), predicateIndex);
    case 32: return expression1Sempred(antlrcpp::downCast<Expression1Context *>(context), predicateIndex);
    case 33: return expression2Sempred(antlrcpp::downCast<Expression2Context *>(context), predicateIndex);
    case 34: return expression3Sempred(antlrcpp::downCast<Expression3Context *>(context), predicateIndex);
    case 35: return expression4Sempred(antlrcpp::downCast<Expression4Context *>(context), predicateIndex);
    case 36: return expression5Sempred(antlrcpp::downCast<Expression5Context *>(context), predicateIndex);
    case 37: return expression6Sempred(antlrcpp::downCast<Expression6Context *>(context), predicateIndex);
    case 38: return expression7Sempred(antlrcpp::downCast<Expression7Context *>(context), predicateIndex);
    case 39: return expression8Sempred(antlrcpp::downCast<Expression8Context *>(context), predicateIndex);
    case 40: return expression9Sempred(antlrcpp::downCast<Expression9Context *>(context), predicateIndex);
    case 41: return expression10Sempred(antlrcpp::downCast<Expression10Context *>(context), predicateIndex);
    case 42: return expression11Sempred(antlrcpp::downCast<Expression11Context *>(context), predicateIndex);
    case 49: return function_callSempred(antlrcpp::downCast<Function_callContext *>(context), predicateIndex);
    case 50: return left_valueSempred(antlrcpp::downCast<Left_valueContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool gscParser::expression0Sempred(Expression0Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression1Sempred(Expression1Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 1: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression2Sempred(Expression2Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 2: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression3Sempred(Expression3Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 3: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression4Sempred(Expression4Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 4: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression5Sempred(Expression5Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 5: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression6Sempred(Expression6Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 6: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression7Sempred(Expression7Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 7: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression8Sempred(Expression8Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 8: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression9Sempred(Expression9Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 9: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression10Sempred(Expression10Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 10: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::expression11Sempred(Expression11Context *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 11: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool gscParser::function_callSempred(Function_callContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 12: return precpred(_ctx, 1);

  default:
    break;
  }
  return true;
}

bool gscParser::left_valueSempred(Left_valueContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 13: return precpred(_ctx, 4);
    case 14: return precpred(_ctx, 3);

  default:
    break;
  }
  return true;
}

void gscParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  gscParserInitialize();
#else
  ::antlr4::internal::call_once(gscParserOnceFlag, gscParserInitialize);
#endif
}
