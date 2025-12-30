
/* (c)  oblong industries */

#define _USE_MATH_DEFINES  //will get M_PI out of math.h on windows
#include "libPlasma/c/slaw-ordering.h"
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "libPlasma/c/slaw.h"
#include "libPlasma/c/protein.h"
#include "libPlasma/c/slaw-io.h"
#include "libPlasma/c/slaw-string.h"
#include "libLoam/c/ob-log.h"

static const char *japanese[] = {
  "There are three types of adjective:",
  "",
  "1) \345\275\242\345\256\271\350\251\236 keiy\305\215shi, or i "
  "adjectives, which have a conjugating ending i",
  "   (\343\201\204) (such as \343\201\202\343\201\244\343\201\204 atsui "
  "\"to be hot\") which can become past (\343\201\202\343\201\244",
  "   \343\201\213\343\201\243 \343\201\237 atsukatta \"it was hot\"), or "
  "negative (\343\201\202\343\201\244\343\201\217\343\201\252\343\201\204 "
  "atsuku nai",
  "   \"it is not hot\"). Note that nai is also an i adjective, which can",
  "   become past (\343\201\202\343\201\244\343\201\217\343\201\252"
  "\343\201\213\343\201\243\343\201\237 atsuku nakatta \"it was not hot\").",
  "",
  "      \346\232\221\343\201\204\346\227\245 atsui hi \"a hot day\".",
  "",
  "2) \345\275\242\345\256\271\345\213\225\350\251\236 "
  "keiy\305\215d\305\215shi, or na adjectives, which are followed by a form",
  "   of the copula, usually na. For example hen (strange)",
  "",
  "      \345\244\211\343\201\252\343\201\262\343\201\250 hen na hito \"a "
  "strange person\".",
  "",
  "3) \351\200\243\344\275\223\350\251\236 rentaishi, also called true "
  "adjectives, such as ano \"that\"",
  "",
  "      \343\201\202\343\201\256\345\261\261 ano yama \"that mountain\".",
  ""};

static const char *plasma[] = {
  "-- Description: Proteins, Pools, & Slaw --",
  "",
  "This text describes initial libPlasma components used for exchange of",
  "data within and between computing processes.",
  "",
  "A \"process,\" in our usage, means a computer program executing for a",
  "period of time. This execution may be interrupted by pauses or",
  "transfers between processors or machines, so that a process is",
  "possibly suspended indefinitely and \"serialized\" or \"marshaled\" to",
  "disk, across a memory bus, or over a network.",
  "",
  "The present work's context is a new programming environment that",
  "permits (indeed: encourages) large scale multi-process",
  "interoperation. Existing programming environments do not fully support",
  "multi-cpu and cross-network execution, or flexible sharing of data",
  "between large numbers of computing processes.",
  "",
  "In pursuit of this goal we have designed and implemented several",
  "constructs that together enable",
  "",
  " - Efficient exchange of data between large numbers of processes",
  " - Flexible data \"typing\" and structure, so that widely varying kinds",
  "   and uses of data are supported",
  " - Flexible mechanisms for data exchange (local memory, disk,",
  "   network, etc.), all driven by substantially similar APIs.",
  " - Data exchange between processes written in different programming",
  "   languages",
  " - Automatic maintenance of data caching and aggregate state",
  "",
  "The principal constructs include slawx (plural of \"slaw\"), a mechanism",
  "for efficient, platform-independent data representation and access;",
  "proteins, a data encapsulation and transport scheme (whose payload is",
  "generally and ideally slawx); and pools, which provide structured yet",
  "flexible aggregation, ordering, filtering, and distribution of",
  "proteins -- within a process, among local processes, across a network",
  "between remote or distributed processes, and via 'longer term'",
  "(e.g. on-disk) storage.",
  "",
  "",
  "- Proteins -",
  "",
  "The \"protein\" is a new mechanism for encapsulating data that needs to",
  "be shared between processes (or moved across a bus or network).",
  "",
  "A protein is a structured record format and an associated set of",
  "methods for manipulating records (putting data in, taking data out,",
  "querying the format and existence of data). Proteins are designed to",
  "be used via code written in a variety of computer languages; to be the",
  "basic building block for \"pools\", described below; and to be natively",
  "able to move between processors and across networks while maintaining",
  "intact the data they contain.",
  "",
  "One immediate -- and important -- use of proteins is as an improved",
  "mechanism for transport and manipulation of user interface \"events.\"",
  "Today's mainstream, user-facing computing platforms (OS X, Microsoft",
  "Windows, X Windows) provide facilities for transmitting user interface",
  "event data between processes. But these existing mechanisms all suffer",
  "from several major design shortcomings that make it difficult to build",
  "multi-process and multi-machine applications, and that force users",
  "working in more than one programming language to jump through",
  "frustrating hoops. Existing event frameworks are",
  "",
  " - strongly typed, which makes them inflexible, privileges the",
  "   assumptions of the systems vendor over the application programmer,",
  "   and forms a mis-match with the facilities of increasingly popular",
  "   dynamic languages (such as Ruby, Python and Perl)",
  " - point-to-point, which makes coordinating the activity of more than",
  "   a few distinct processes difficult or impossible",
  " - strongly dependent on particular local, in-memory data structures,",
  "   which renders them unsuited for on-disk storage or transmission",
  "   across a network",
  "",
  "In contrast, proteins are untyped (but provide a powerful and flexible",
  "pattern-matching facility, on top of which \"type-like\" functionality",
  "is implemented); are inherently multi-point (although point-to-point",
  "forms are easily implemented as a subset of multi-point transmission);",
  "and define a \"universal\" record format that does not differ (or",
  "differs only in the types of optional optimizations that are",
  "performed) between in-memory, on-disk, and on-the-wire (network)",
  "formats.",
  "",
  "A protein is a linear sequence of bytes. Within these bytes are",
  "encapsulated a \"descrips\" list -- an arbitrarily elaborate but",
  "efficiently filterable per-protein description -- and a set of",
  "key-value pairs called \"ingests\" -- the actual \"contents\" of the",
  "protein.",
  "",
  "A minimal (and read-only) protein implementation might define the",
  "following behaviors in one or more programming languages:",
  "",
  " - query the length in bytes of a protein",
  " - query the number of descrips entries",
  " - query the number of ingests",
  " - retrieve a descrip entry by index number",
  " - retrieve an ingest by index number",
  "",
  "In addition, most implementations also define basic methods allowing",
  "proteins to be constructed and filled with data, helper-methods that",
  "make common tasks easier for programmers, and hooks for creating",
  "optimizations:",
  "",
  " - create a new protein",
  " - append a series of descrips entries",
  " - append an ingest",
  " - query the presence of a matching descrip",
  " - query the presence of a matching ingest key",
  " - retrieve an ingest value given a key",
  " - pattern match across descrips",
  " - delete one or more descrips",
  " - delete one or more ingests",
  " - embed non-structured metadata near the beginning of a protein",
  "",
  "Proteins' concern with key-value pairs, as well as some core ideas",
  "about network-friendly and multi-point data interchange, are shared",
  "with earlier systems that privilege the concept of \"tuples\" (examples",
  "include Linda and Jini). Proteins differ from tuple-oriented systems",
  "in several major ways, including the use of the descrips list to",
  "provide a standard, optimizable pattern matching substrate; and the",
  "rigorous specification of a record format appropriate for a variety of",
  "storage and language constructs (along with several particular",
  "implementations of \"interfaces\" to that record format).",
  "",
  "Following is a somewhat detailed account of proteins' present",
  "specification. Note that the specification is still subject to",
  "occasional adjustments and improvements; the account is presented here",
  "merely by way of illustrating the system's character.",
  "",
  "The first four or eight bytes of a protein specify the protein's",
  "length, which must be a multiple of 16 bytes. This 16-byte",
  "granularity ensures that byte- and bus-alignment efficiencies are",
  "achievable on contemporary hardware. A protein that is not naturally",
  "\"quad-word aligned\" must be padded with arbritrary bytes so that its",
  "length is a multiple of 16 bytes.",
  "",
  "The length portion of a protein has the following format: 32 bits",
  "specifying length, in big-endian format, with the four lowest-order",
  "bits serving as flags to indicate macro-level protein structure",
  "characteristics; followed by 32 further bits if the protein's length",
  "is greater than 2^32 bytes.",
  "",
  "The 16-byte-alignment proviso means that the lowest order bits of the",
  "first four bytes are available as flags. And so the first three",
  "low-order bit flags indicate whether the protein's length can be",
  "expressed in the first four bytes or requires eight; whether the",
  "protein uses big-endian or little-endian byte ordering; and whether",
  "the protein employs standard or non-standard structure. The fourth",
  "flag bit is reserved for future use.",
  "",
  "If the eight-byte length flag bit is set, the length of the protein is",
  "calculated by reading the next four bytes and using them as the",
  "high-order bytes of a big-endian, eight-byte integer (with the four",
  "bytes already read supplying the low-order portion).",
  "",
  "If the little-endian flag is set, all binary numerical data in the",
  "protein is to be interpreted as little-endian (otherwise,",
  "big-endian). If the non-standard flag bit is set, the remainder of the",
  "protein does not conform to the standard structure to be described",
  "below.",
  "",
  "We will not further discuss non-standard protein structures, except to",
  "say that there are various methods for describing and synchronizing on",
  "non-standard protein formats available to a systems programmer using",
  "proteins and pools, and that these methods can be useful when space or",
  "compute cycles are constrained. For example, the shortest protein is",
  "-- by definition -- sixteen bytes. A standard-format protein cannot",
  "fit any actual payload data into those sixteen bytes (the lion's share",
  "of which is already relegated to describing the location of the",
  "protein's component parts). But a non-standard format protein could",
  "conceivably use 12 of its 16 bytes for data. And two applications",
  "exchanging proteins could mutually decide that any 16-byte-long",
  "proteins that they emit always contain 12 bytes representing, for",
  "example, 12 8-bit sensor values from a real-time analog-to-digital",
  "convertor.",
  "",
  "The remainder of this section describes a standard protein's layout.",
  "",
  "Immediately following the length header, two more variable-length",
  "integer numbers appear. These numbers specify offsets to,",
  "respectively, the first element in the descrips list and the first",
  "key-value pair. The byte order of each quad of these numbers is",
  "specified by the protein's endianness flag bit. For each, the most",
  "significant bit of the first four bytes determines whether the number",
  "is four or eight bytes wide. If the msb is set, the first four bytes",
  "are the most significant bytes of a double-word (eight byte)",
  "number. We will refer to this as \"offset form\".",
  "",
  "The presence of these two offsets at the beginning of a protein allows",
  "for several useful optimizations.",
  "",
  "Most proteins will not be so large as to require eight-byte lengths or",
  "pointers, so in general the length (with flags) and two offset numbers",
  "will occupy only the first three bytes of a protein. "
  "On many hardware",
  "architectures, a fetch or read of a certain number of bytes beyond the",
  "first is \"free\" (16 bytes, for example, take exactly the same number",
  "of clock cycles to pull across the Cell processor's main bus as a",
  "single byte).",
  "",
  "In many instances it is useful to allow implementation- or",
  "context-specific caching or metadata inside a protein. The use of",
  "offsets allows for a flexibly-sized \"hole\" to be created near the",
  "beginning of the protein, into which such metadata may be slotted. An",
  "implementation that can make use of eight bytes of metadata gets those",
  "bytes for free on many architectures with every fetch of the length",
  "header for a protein.",
  "",
  "Use of separate offsets pointing to descrips and pairs allows descrips",
  "and pairs to be handled by different code paths, making possible",
  "particular optimizations relating to, for example, descrips",
  "pattern-matching and protein assembly.",
  "",
  "The descrips offset specifies the number of bytes between the",
  "beginning of the protein and the first descrip entry. Each descrip",
  "entry consists of an offset (in offset form, of course) to the next",
  "descrip entry, followed by a variable-width length field (again in",
  "offset format), followed by a \"slaw\" (discussed separately, below). If",
  "there are no further descrips, the offset is, by rule, four bytes of",
  "zeros. Otherwise, the offset specifies the number of bytes between the",
  "beginning of this descrip entry and the next one. The length field",
  "specifies the length of the slaw, in bytes.",
  "",
  "In most proteins, each descrip is a string, formatted in the slaw",
  "string fashion: a four-byte length/type header with the most",
  "significant bit set and only the lower 30 bits used to specify length,",
  "followed by the header's indicated number of data bytes. As usual, the",
  "length header takes its endianness from the protein. Bytes are assumed",
  "to encode UTF-8 characters (and thus -- nota bene -- the number of",
  "characters is not necessarily the same as the number of bytes).",
  "",
  "The duplication of length data in both the protein descrip entry",
  "length field and the slaw string header is a candidate for",
  "optimization.",
  "",
  "The ingests offset specifies the number of bytes between the beginning",
  "of the protein and the first ingest entry. Each ingest entry consists",
  "of an offset (in offset form) to the next ingest entry, followed again",
  "by a length field and a slaw. The ingest offset is functionally",
  "identical to the descrip offset, except that it points to the next",
  "ingest entry rather than to the next descrip entry.",
  "",
  "In most proteins, every ingest is of the slaw cons type (a two-value",
  "list, generally used as a key/value pair). The slaw cons record",
  "consists of a four-byte length/type header with the second most",
  "significant bit set and only the lower 30 bits used to specify length;",
  "a four-byte offset to the start of the value (second) element; the",
  "four-byte length of the key element; the slaw record for the key",
  "element; the four-byte length of the value element; and finally the",
  "slaw record for the value element.",
  "",
  "Generally, the cons key is a slaw string. The duplication of data",
  "across the several protein and slaw cons length and offsets field",
  "provides yet more opportunity for refinement and optimization.",
  "",
  "",
  "- Slawx -",
  "",
  "The construct used to embed typed data inside proteins, as described",
  "above, is a tagged byte-sequence specification and abstraction called",
  "a \"slaw.\" (Plural: slawx.)",
  "",
  "A slaw is a linear sequence of bytes representing a piece of (possibly",
  "aggregate) typed data, and is associated with",
  "programming-language-specific APIs that allow slawx to be created,",
  "modified and moved around between memory spaces, storage media, and",
  "machines. The slaw type scheme is intended to be extensible and as",
  "lightweight as possible, and to be a common substrate that can be",
  "used from any programming language.",
  "",
  "The desire to build an efficient, large-scale inter-process",
  "communication mechanism is the driver of the slaw design. Modern",
  "programming languages provide sophisticated data structures and type",
  "facilities that work beautifully in process-specific memory layouts,",
  "but these data representations invariably break down when data needs",
  "to be moved between processes or stored on disk. The slaw architecture",
  "is, first, a substantially efficient, multi-platform friendly,",
  "low-level data model for inter-process communication.",
  "",
  "But even more importantly, slawx are designed to influence -- together",
  "with proteins -- the development of future computing hardware",
  "(microprocessors, memory controllers, disk controllers). A few",
  "specific additions to, say, the instruction sets of commonly available",
  "microprocessors would make it possible for slawx to become as",
  "efficient even for single-process, in-memory data layout as the schema",
  "used in most programming languages.",
  "",
  "Every slaw consists of a variable-length type header followed by a",
  "type-specific data layout. In the current implementation, which",
  "supports full slaw functionality in C, C++ and Ruby, types are",
  "indicated by a universal integer defined in system header files",
  "accessible from each language. More sophisticated and flexible type",
  "resolution functionality is anticipated: for example, indirect typing",
  "via universal object IDs and network lookup.",
  "",
  "Basic slaw functionality includes API facilities to",
  "",
  " - create a new slaw of a specific type",
  " - \"create\" (build a language-specific reference to) a slaw from",
  "   bytes on disk or in memory",
  " - embed data within a slaw in type-specific fashion",
  " - retrieve data from within a slaw",
  " - clone (shallow-copy) a slaw",
  " - copy (deep-copy) a slaw",
  " - translate the endianness of all data within a slaw",
  "",
  "Every species of slaw is required to implement the above behaviors.",
  "",
  "Our slaw implementation allows slaw records to be used as objects in",
  "language-friendly fashion from both Ruby and C++. A suite of utilities",
  "external to the C++ compiler sanity-check slaw byte layout, create",
  "header files and macros specific to individual slaw types, and",
  "auto-generate bindings for Ruby. As a result, well-designed slaw types",
  "are quite efficient even when used from within a single process. Any",
  "slaw anywhere in a process's accessible memory can be addressed",
  "without a copy or \"deserialization\" step.",
  "",
  "",
  "- Pools -",
  "",
  "A \"pool\" is a repository for proteins, providing linear sequencing and",
  "state caching; multi-process access; and a set of common, optimizable",
  "filtering and pattern-matching behaviors.",
  "",
  "Slawx provide the lowest-level of data definition for inter-process",
  "exchange; proteins provide mid-level structure and hooks for querying",
  "and filtering; and pools provide for high-level organization and",
  "access semantics.",
  "",
  "The pools API is designed to allow pools to be implemented in a",
  "variety of ways, in order to account both for system-specific goals",
  "and for the available capabilities of given hardware and network",
  "architectures. The two fundamental system provisions upon which pools",
  "depend are a storage facility and a means of inter-process",
  "communication. In our extant systems we use a flexible combination of",
  "shared memory, virtual memory, and disk for the former, and IPC queues",
  "and TCP/IP sockets for the latter.",
  "",
  "The most basic pool functionality is as follows:",
  "",
  " - \"participate in\" a pool",
  " - put a protein in a pool",
  " - retrieve the next unseen protein from a pool",
  " - \"rewind\" or \"fast-forward\" within a pool",
  "",
  "A very simple pool implementation might provide only those four",
  "methods. However, most real-world pools would also support (and",
  "encourage):",
  "",
  " - setting up a \"streaming\" pool call-back for a process",
  " - selectively retrieving proteins that match particular patterns of",
  "   descrips or ingests keys",
  " - scanning backward and forwards for proteins that match particular",
  "   patterns of descrips or ingests keys",
  "",
  "Critical to the design of the pools architecture is the conception of",
  "pools as maintaining state, so that individual processes can offload",
  "much of the tedious bookkeeping common to multi-process program",
  "code. A pool attempts to keep a large buffer of past proteins",
  "available -- the Platonic pool is explicitly infinite -- so that",
  "participating processes can scan both backwards and forwards in a pool",
  "at will. The size of the buffer is implentation dependent, of course,",
  "but in common usage it is often possible to keep proteins in a pool",
  "for hours or days.",
  "",
  "With so much data available to potentially legion pool participants,",
  "pool behavior optimization becomes important. We have implemented a",
  "number of optimized caching and storage schemes that make it possible",
  "to accomodate tens of thousands of proteins in a pool and, and we use",
  "careful connection pooling on the unix operating system to handle",
  "hundreds of concurrent connections with relatively low overhead. As",
  "with slawx, we expect eventually to implement many of these algorithms",
  "in hardware, leading to further efficiency and scalability.",
  "",
  "The most common \"style\" of pool usage in our programs hews to a",
  "biological metaphor, in contrast to the mechanistic, point-to-point",
  "approach taken by existing inter-process communication frameworks. The",
  "name \"protein\" alludes to bilological inspiration: data proteins in",
  "pools are available for flexible querying and pattern matching by a",
  "large number of computational processes, as chemical proteins in a",
  "living organism are available for pattern matching and filtering by",
  "large numbers of cellular agents.",
  "",
  "Two additional abstractions (currently implemented in both",
  "C++ and Ruby) lean on the biological metaphor.",
  "",
  "A process that participates in a pool generally creates a number of",
  "\"handlers,\" small bundles of code that associate \"match conditions\"",
  "with \"handle behaviors.\" By tying one or more handlers to a pool, a",
  "process sets up flexible call-back triggers that are trivially able to",
  "encapsulate state and react to new proteins.",
  "",
  "A process that participates in several pools generally inherits from",
  "an abstract \"Golgi\" class. The Golgi framework provides a number of",
  "useful routines for managing multiple pools and handlers. The Golgi",
  "class also encapsualates parent-child relationships, providing a",
  "mechanism for \"local\" protein exchange that does not use a",
  "pool.",
  ""};

static const byte bz2_data[] = {
  0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0x58, 0x1f, 0x5a,
  0x35, 0x00, 0x00, 0x4f, 0xff, 0x80, 0x7f, 0xfe, 0xc2, 0x00, 0x40, 0xf7, 0x7f,
  0xd8, 0x10, 0x02, 0x08, 0x00, 0x3f, 0xff, 0xff, 0xf0, 0x50, 0x03, 0xb8, 0x33,
  0x3d, 0x77, 0x5d, 0x57, 0x4e, 0xb6, 0xb0, 0x34, 0xd2, 0x13, 0x4c, 0x29, 0x9a,
  0x46, 0x93, 0x4d, 0xa8, 0xf5, 0x06, 0x80, 0x06, 0x83, 0x43, 0x40, 0x00, 0xd0,
  0x99, 0x04, 0xc8, 0x05, 0x32, 0x1a, 0x68, 0x06, 0x80, 0x1a, 0x68, 0x01, 0xa6,
  0x40, 0x24, 0x44, 0x4d, 0x29, 0xe9, 0x31, 0x3d, 0x32, 0x21, 0x32, 0x34, 0xd0,
  0x0d, 0x06, 0x26, 0x83, 0x35, 0x1a, 0x68, 0x1c, 0xc0, 0x26, 0x00, 0x4c, 0x00,
  0x02, 0x60, 0x00, 0x26, 0x00, 0x02, 0x53, 0x4a, 0x34, 0xa7, 0xa0, 0x4f, 0x4d,
  0x41, 0xb4, 0x43, 0x43, 0x04, 0x62, 0x1e, 0x80, 0x98, 0x4c, 0x09, 0xdf, 0xe5,
  0x97, 0x32, 0x4b, 0x56, 0xf8, 0x0a, 0xd5, 0x43, 0x1d, 0x66, 0x54, 0x94, 0xa4,
  0x92, 0x88, 0x83, 0xf2, 0xea, 0xc6, 0xd0, 0x9b, 0x1d, 0x00, 0x88, 0x02, 0x61,
  0x24, 0xa8, 0x92, 0xb9, 0x72, 0xf0, 0xf9, 0x5f, 0x0b, 0xa1, 0xc3, 0xf7, 0x95,
  0x82, 0x8e, 0xe0, 0xb7, 0xa2, 0x3b, 0x69, 0x15, 0xbe, 0x0e, 0xda, 0xc3, 0x75,
  0x63, 0x6d, 0x36, 0x0f, 0xc6, 0xd3, 0x2d, 0x36, 0x5e, 0x00, 0x2c, 0x04, 0x29,
  0x6d, 0x9d, 0xd7, 0xdb, 0xfb, 0x4b, 0x8b, 0x2d, 0x0d, 0x8c, 0x32, 0x0d, 0x7a,
  0x55, 0x69, 0xc5, 0x04, 0x01, 0x8e, 0x56, 0xce, 0xb7, 0x81, 0x72, 0x11, 0x48,
  0x80, 0x60, 0xe9, 0xb6, 0x05, 0x32, 0xc9, 0x66, 0x13, 0x69, 0xbb, 0x67, 0xf3,
  0x5e, 0x02, 0x08, 0x7f, 0x72, 0x33, 0x8e, 0x39, 0xa2, 0x72, 0xfc, 0x9c, 0xed,
  0x2e, 0xfd, 0xf7, 0x6e, 0xd1, 0x5e, 0xe8, 0xaa, 0xce, 0x1b, 0x11, 0xba, 0xec,
  0xf1, 0xd3, 0xa6, 0xf7, 0x86, 0x15, 0xb9, 0xd4, 0x3a, 0x5d, 0x3f, 0x74, 0x88,
  0x73, 0x43, 0x68, 0x3d, 0x76, 0xc7, 0x92, 0x97, 0x8f, 0x2c, 0x6e, 0xa3, 0xe9,
  0x23, 0xaa, 0x28, 0x24, 0x8d, 0x62, 0x82, 0xb8, 0xd0, 0xde, 0x99, 0x05, 0x74,
  0x9b, 0xef, 0x6d, 0x0e, 0x0b, 0x85, 0x11, 0xf8, 0x34, 0x8c, 0x50, 0xe4, 0x40,
  0xad, 0xd7, 0x5c, 0xe2, 0xa7, 0xf3, 0x7b, 0xe6, 0xe9, 0x08, 0x83, 0x7c, 0x43,
  0x26, 0x68, 0x32, 0xc5, 0xd9, 0xa1, 0x12, 0xc5, 0x21, 0xd7, 0x58, 0x56, 0x1a,
  0xaa, 0xd5, 0x11, 0x65, 0x4b, 0x05, 0xb9, 0x94, 0x01, 0x11, 0x26, 0xec, 0xe2,
  0x20, 0x36, 0x4f, 0x1c, 0xc3, 0xf3, 0x0d, 0x40, 0x7b, 0xc3, 0xd9, 0xf8, 0x09,
  0xde, 0xb8, 0xb2, 0xdd, 0x22, 0xd1, 0x84, 0xa0, 0x1d, 0x2a, 0x56, 0x4e, 0xe4,
  0x76, 0xd9, 0x8a, 0x05, 0x53, 0x1b, 0x0c, 0x13, 0xb0, 0x81, 0x81, 0x20, 0x60,
  0xfc, 0xfb, 0xcb, 0x4d, 0x2e, 0x1d, 0xd9, 0x51, 0xa4, 0xa8, 0x0f, 0x9d, 0x0d,
  0xe6, 0x40, 0x1b, 0x27, 0x8e, 0xb5, 0x0c, 0xd7, 0x58, 0xf0, 0x40, 0xeb, 0x36,
  0x09, 0x68, 0xf1, 0x29, 0x5e, 0xd0, 0x1e, 0xd6, 0x51, 0x90, 0xc9, 0x64, 0xda,
  0xea, 0x98, 0x54, 0x20, 0x5d, 0x50, 0x9d, 0x1b, 0x98, 0xae, 0xce, 0x52, 0x18,
  0x35, 0xca, 0x1e, 0xa2, 0x3c, 0x02, 0x02, 0xd2, 0x6f, 0x08, 0x85, 0xe0, 0xd7,
  0x3f, 0xb7, 0xd3, 0xd2, 0x3e, 0xa1, 0xca, 0x4e, 0x17, 0x74, 0xf3, 0xe8, 0x93,
  0x0d, 0x41, 0x54, 0x1f, 0x68, 0xd4, 0x9f, 0x83, 0x06, 0x7a, 0xf6, 0x6c, 0xee,
  0xd9, 0x89, 0x40, 0xc8, 0x7c, 0x81, 0x61, 0x1b, 0xab, 0x52, 0x1c, 0x6c, 0x8a,
  0xc0, 0x75, 0xbe, 0xc3, 0x8b, 0x8f, 0x76, 0xe5, 0x4e, 0xcb, 0x91, 0xda, 0xc3,
  0x58, 0xd9, 0x2b, 0xbc, 0x68, 0x6d, 0xb0, 0x93, 0x43, 0x46, 0x43, 0x83, 0xd0,
  0xe8, 0x1d, 0x39, 0x34, 0x18, 0x8c, 0x94, 0x2a, 0xa0, 0xf2, 0xae, 0x88, 0x9e,
  0x59, 0x67, 0x44, 0xd1, 0xc6, 0x74, 0xec, 0x24, 0x69, 0x0e, 0xb2, 0xca, 0xc4,
  0x8c, 0x65, 0xe5, 0x2d, 0xbc, 0xd5, 0x14, 0x58, 0xce, 0x0a, 0x78, 0x08, 0xde,
  0x83, 0x51, 0x8e, 0x3a, 0x95, 0xd4, 0xd7, 0xad, 0xa0, 0xd1, 0x39, 0xf4, 0x50,
  0x68, 0xc1, 0xac, 0x9d, 0xb0, 0xa9, 0x31, 0x24, 0xc5, 0x0f, 0x58, 0xad, 0x5d,
  0x5a, 0xf1, 0x17, 0x37, 0x78, 0xa8, 0x53, 0xa6, 0x41, 0x20, 0x40, 0x10, 0x9a,
  0x38, 0x52, 0x46, 0x4f, 0x9c, 0x1d, 0xcd, 0x9a, 0x50, 0xab, 0x54, 0x4a, 0xe3,
  0x0c, 0x5c, 0x1c, 0x18, 0x49, 0xc9, 0x7a, 0xb8, 0xcf, 0x02, 0x76, 0xdf, 0x00,
  0x2e, 0x29, 0x72, 0x64, 0xbd, 0x82, 0x6a, 0x90, 0x90, 0x0d, 0x20, 0xcd, 0xe8,
  0x16, 0xd7, 0xde, 0x03, 0x12, 0x45, 0xb3, 0x54, 0xbd, 0x06, 0x61, 0xf1, 0x16,
  0xc4, 0x8a, 0xe2, 0xae, 0x80, 0xa8, 0xde, 0x88, 0x05, 0xb2, 0xa7, 0x53, 0x2a,
  0x5d, 0x54, 0x5a, 0xd3, 0xb4, 0x6c, 0xac, 0x44, 0x83, 0x22, 0x88, 0x61, 0x1c,
  0x35, 0xd3, 0x2a, 0x8c, 0xb9, 0x11, 0x6b, 0x61, 0x68, 0xa2, 0xa3, 0xab, 0x76,
  0x6c, 0x2a, 0xc1, 0xb4, 0x7c, 0x5d, 0xf6, 0xc6, 0xbb, 0x77, 0x46, 0x2e, 0x3b,
  0x9d, 0x13, 0xbc, 0x36, 0x7a, 0x84, 0x91, 0xa1, 0x6a, 0x26, 0x55, 0x20, 0xcb,
  0x10, 0x60, 0x2b, 0x8a, 0x67, 0x37, 0x54, 0x59, 0x60, 0x72, 0x17, 0x48, 0x57,
  0x1c, 0xbd, 0x74, 0x5a, 0xe5, 0x1b, 0xc0, 0x25, 0xa4, 0xba, 0xda, 0x4b, 0x2d,
  0x25, 0x04, 0x0c, 0xc1, 0x43, 0xcd, 0x9b, 0x66, 0x49, 0x1b, 0x54, 0x43, 0x8c,
  0x5b, 0x6a, 0x36, 0xad, 0x39, 0xdc, 0x02, 0x5e, 0x8a, 0xa3, 0x33, 0x52, 0x20,
  0x68, 0x43, 0xb4, 0x8a, 0x2e, 0xb9, 0x3a, 0xb8, 0xe4, 0xd0, 0x52, 0xd2, 0x2c,
  0x6d, 0x1c, 0xa1, 0x60, 0xf8, 0x64, 0x6c, 0xfd, 0x4b, 0x2c, 0xd5, 0x03, 0x9a,
  0x94, 0xcd, 0x50, 0x6e, 0x19, 0x50, 0x66, 0x40, 0xac, 0xb3, 0x91, 0x87, 0x31,
  0x17, 0x01, 0xe2, 0x70, 0xe2, 0x79, 0x36, 0xef, 0x10, 0xa3, 0x2b, 0x44, 0x9a,
  0x2b, 0x57, 0xd0, 0x0a, 0x90, 0xdd, 0x89, 0x62, 0xa2, 0x99, 0x04, 0xfd, 0x29,
  0x64, 0xd4, 0x0a, 0x4f, 0x76, 0x4d, 0xa8, 0xe0, 0xaa, 0x16, 0x4d, 0x13, 0x0f,
  0x9c, 0x5c, 0xc1, 0x64, 0x83, 0x72, 0x28, 0x6a, 0xd9, 0x4f, 0xf4, 0xed, 0x59,
  0xdc, 0xba, 0x00, 0x66, 0x2e, 0x8f, 0x83, 0x2b, 0x1b, 0x84, 0x20, 0xf9, 0x31,
  0x36, 0x86, 0xeb, 0xb1, 0x42, 0x04, 0x12, 0xd0, 0xc6, 0xd9, 0x2d, 0x24, 0x94,
  0x24, 0x74, 0xb5, 0x29, 0xa6, 0x9a, 0x1a, 0x10, 0xb3, 0x5a, 0xb9, 0x30, 0x98,
  0x6f, 0xcc, 0x8c, 0xc2, 0xe3, 0xfe, 0x2e, 0xe4, 0x8a, 0x70, 0xa1, 0x20, 0xb0,
  0x3e, 0xb4, 0x6a};

static const float64 square_roots[] = {2,
                                       1.4142135623730951,
                                       1.189207115002721,
                                       1.0905077326652577,
                                       1.0442737824274138,
                                       1.0218971486541166,
                                       1.0108892860517005,
                                       1.0054299011128027,
                                       1.0027112750502025,
                                       1.0013547198921082,
                                       1.0006771306930664,
                                       1.0003385080526823,
                                       1.0001692397053021,
                                       1.0000846162726942,
                                       1.0000423072413958,
                                       1.0000211533969647,
                                       1.0000105766425498,
                                       1.0000052883072919,
                                       1.0000026441501502,
                                       1.0000013220742012,
                                       1.0000006610368821,
                                       1.0000003305183864,
                                       1.0000001652591797,
                                       1.0000000826295865,
                                       1.0000000413147925,
                                       1.000000020657396,
                                       1.0000000103286979,
                                       1.0000000051643489,
                                       1.0000000025821745,
                                       1.0000000012910872,
                                       1.0000000006455436,
                                       1.0000000003227718,
                                       1.0000000001613858,
                                       1.0000000000806928,
                                       1.0000000000403464,
                                       1.0000000000201732,
                                       1.0000000000100866,
                                       1.0000000000050433,
                                       1.0000000000025215,
                                       1.0000000000012608,
                                       1.0000000000006304,
                                       1.0000000000003151,
                                       1.0000000000001574,
                                       1.0000000000000786,
                                       1.0000000000000393,
                                       1.0000000000000195,
                                       1.0000000000000098,
                                       1.0000000000000049,
                                       1.0000000000000024,
                                       1.0000000000000011,
                                       1.0000000000000004,
                                       1.0000000000000002};

static slaw stringify (const char *const *strings, int64 nstrings)
{
  slabu *bu = slabu_new ();
  int64 i;
  for (i = 0; i < nstrings; i++)
    slabu_list_add_c (bu, strings[i]);
  return slaw_strings_join_slabu_f (bu, "\n");
}

#define STRINGIFY(x) stringify ((x), sizeof (x) / sizeof ((x)[0]))

static protein make_an_interesting_protein (void)
{
  slabu *descrips;
  slabu *ingests;
  protein p;
  slabu *sb;
  unt16 phone[] = {213, 683, 8863};
  v3int8 units[6];
  v4float64c cplx;
  unt32c cplx1;
  int i;
  int n;
  int f;

  descrips = slabu_new ();
  ingests = slabu_new ();
  slabu_list_add_c (descrips, ":mouse");
  slabu_list_add_c (descrips, ":usb");
  slabu_list_add_c (descrips, ":0x347689");
  slabu_list_add_c (descrips, ":down");
  slabu_map_put_cf (ingests, "hello", slaw_string ("world"));
  p = protein_from_ff (slaw_list (descrips), slaw_list (ingests));
  slabu_map_put_cf (ingests, "a nested protein", p);
  sb = slabu_new ();
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_unt16 (923)));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("E.")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_unt8 (3)));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_string ("rd St. Unit")));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_unt8 (111)));
  OB_DIE_ON_ERROR (slabu_list_add_x (
    sb, slaw_cons_ff (slaw_string ("Los Angeles"), slaw_string ("CA"))));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_unt32 (90013)));
  OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_unt16_array (phone, 3)));
  slabu_map_put_cf (ingests, "Address", slaw_list_f (sb));
  slabu_map_put_cf (ingests, " the final frontier",
                    slaw_cons_ff (slaw_string ("NCC"), slaw_int16 (-1701)));
  slabu_map_put_cf (ingests, "a large block of text", STRINGIFY (plasma));
  slabu_map_put_cf (ingests, "some non-ASCII text from Wikipedia",
                    STRINGIFY (japanese));
  memset (units, 0, sizeof (units));
  units[0].x = 1;
  units[1].y = 1;
  units[2].z = 1;
  units[3].x = -1;
  units[4].y = -1;
  units[5].z = -1;
  slabu_map_put_cf (ingests, "my six favorite unit vectors",
                    slaw_v3int8_array (units, 6));
  slabu_map_put_cf (ingests, "pumpkin", slaw_float32 ((float) M_PI));
  slabu_map_put_cf (ingests, "pecan", slaw_float64 (M_PI));
  cplx1.re = 1.0;
  cplx1.im = 0.0;
  slabu_map_put_cf (ingests, "keepin' it real", slaw_unt32c (cplx1));
  cplx.x.re = 1.2;
  cplx.x.im = 3.4;
  cplx.y.re = 5.6;
  cplx.y.im = 7.8;
  cplx.z.re = 9.0;
  cplx.z.im = 0.1;
  cplx.w.re = 0.01;
  cplx.w.im = M_PI;
  slabu_map_put_cf (ingests, "now here's something complex",
                    slaw_v4float64c (cplx));
  slabu_map_put_cf (ingests, "the largest integer",
                    slaw_unt64 (OB_CONST_U64 (18446744073709551615)));
  slabu_map_put_cf (ingests, "the smallest integer",
                    slaw_int64 (OB_CONST_I64 (-9223372036854775807) - 1));
  sb = slabu_new ();
  for (i = 0; i < 5; i++)
    OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_nil ()));
  slabu_map_put_cf (
    ingests, "some strange consing",
    slaw_cons_ff (
      slaw_list_f (sb),
      slaw_cons_ff (slaw_cons_ff (slaw_string ("a"), slaw_string ("b")),
                    slaw_cons_ff (slaw_string ("c"), slaw_string ("d")))));
  sb = slabu_new ();
  slabu_map_put_cf (ingests, "an empty list", slaw_list_f (sb));
  slabu_map_put_cf (
    ingests,
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    slaw_string ("This key is longer than 1024 characters, to test out "
                 "the fact that implicit keys cannot exceed 1024 "
                 "characters, even if the key could otherwise be "
                 "implicit.  See http://yaml.org/spec/1.2/#id2597711 for "
                 "more information.  So that means that we should see a "
                 "'?' in front of this key.  It looks like libYaml adds "
                 "the '?' for any key longer than 128 characters, even "
                 "though the spec says 1024."));
  slabu_map_put_cf (ingests,
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    slaw_unt32 (1023));
  slabu_map_put_cf (ingests,
                    "\360\245\205\275 is outside the Basic Multilingual Plane.",
                    slaw_nil ());
  slabu_map_put_cf (ingests,
                    "$ \302\242 \342\202\245 \302\243 \302\245 "
                    "\342\202\254 \342\202\251",
                    slaw_string ("money"));

  slabu_map_put_cf (ingests, "0-length array of int8",
                    slaw_int8_array_empty (0));
  slabu_map_put_cf (ingests, "0-length array of unt64",
                    slaw_unt64_array_empty (0));
  slabu_map_put_cf (ingests, "0-length array of complex",
                    slaw_float32c_array_empty (0));
  slabu_map_put_cf (ingests, "0-length array of 2-vectors",
                    slaw_v2float64_array_empty (0));
  slabu_map_put_cf (ingests, "0-length array of 3-vectors",
                    slaw_v3unt8_array_empty (0));
  slabu_map_put_cf (ingests, "0-length array of 4-vectors",
                    slaw_v4float32_array_empty (0));
  slabu_map_put_cf (ingests, "0-length array of complex vectors",
                    slaw_v4int16c_array_empty (0));

  slabu_map_put_cf (ingests, "1-length array of int8",
                    slaw_int8_array_empty (1));
  slabu_map_put_cf (ingests, "1-length array of unt64",
                    slaw_unt64_array_empty (1));
  slabu_map_put_cf (ingests, "1-length array of complex",
                    slaw_float32c_array_empty (1));
  slabu_map_put_cf (ingests, "1-length array of 2-vectors",
                    slaw_v2float64_array_empty (1));
  slabu_map_put_cf (ingests, "1-length array of 3-vectors",
                    slaw_v3unt8_array_empty (1));
  slabu_map_put_cf (ingests, "1-length array of 4-vectors",
                    slaw_v4float32_array_empty (1));
  slabu_map_put_cf (ingests, "1-length array of complex vectors",
                    slaw_v4int16c_array_empty (1));

  slabu_map_put_cf (
    ingests, "a map of stuff",
    slaw_map_inline_ff (slaw_string ("false"), slaw_boolean (false),
                        slaw_string ("protein with neither "
                                     "descrips nor ingests"),
                        protein_from_ff (NULL, NULL),
                        slaw_string ("protein with no descrips"),
                        protein_from_ff (NULL, slaw_float64 (3.14159)),
                        slaw_string ("protein with no ingests"),
                        protein_from_ff (slaw_unt32 (0x4ffe874), NULL),
                        slaw_string ("true"), slaw_boolean (true), NULL));

  n = 0;
  f = 0;
  sb = slabu_new ();
  for (i = 0; i < 3; i++)
    {
      slabu *sb2 = slabu_new ();
      int j;

      for (j = 0; j < 3; j++)
        {
          slabu *sb3 = slabu_new ();
          int k;

          for (k = 0; k < 3; k++, n++)
            {
              float64 a[27];
              int l;

              for (l = 0; l < n; l++)
                {
                  a[l] = 1;
                  if (f < sizeof (square_roots) / sizeof (square_roots[0]))
                    a[l] = square_roots[f];
                  f++;
                }

              OB_DIE_ON_ERROR (
                slabu_list_add_x (sb3, slaw_float64_array (a, n)));
            }

          OB_DIE_ON_ERROR (slabu_list_add_x (sb2, slaw_list_f (sb3)));
        }

      OB_DIE_ON_ERROR (slabu_list_add_x (sb, slaw_list_f (sb2)));
    }

  slabu_map_put_cf (ingests, "HorriblyNestedLists", slaw_list_f (sb));

  return protein_from_ffr (slaw_list_f (descrips), slaw_list_f (ingests),
                           bz2_data, sizeof (bz2_data));
}

static int read_test (const char *filename)
{
  protein p, q;
  ob_retort err;
  int c;

  err = slaw_read_from_binary_file (filename, &q);
  if (err != OB_OK)
    {
      fprintf (stderr, "error reading protein: %s\n", ob_error_string (err));
      return EXIT_FAILURE;
    }

  p = make_an_interesting_protein ();
  if ((c = slaw_semantic_compare (p, q)) == 0)
    {
      protein_free (p);
      protein_free (q);
      return EXIT_SUCCESS;
    }
  else
    {
      const char *p_file = "scratch/et-expected.spew";
      const char *q_file = "scratch/et-actual.spew";
      FILE *f = fopen (p_file, "w");
      if (!f)
        OB_FATAL_ERROR_CODE (0x20300000, "Couldn't open %s\n", p_file);
      slaw_spew_overview (p, f, NULL);
      fclose (f);
      f = fopen (q_file, "w");
      if (!f)
        OB_FATAL_ERROR_CODE (0x20300001, "Couldn't open %s\n", q_file);
      slaw_spew_overview (q, f, NULL);
      fclose (f);
      fprintf (stderr, "protein_semantic_compare returned %d; see %s %s\n", c,
               p_file, q_file);
      return EXIT_FAILURE;
    }
}

static int write_protein (const char *filename)
{
  protein p = make_an_interesting_protein ();
  ob_retort err;

  err = slaw_write_to_binary_file (filename, p);
  if (err != OB_OK)
    {
      fprintf (stderr, "error writing protein: %s\n", ob_error_string (err));
      return EXIT_FAILURE;
    }

  protein_free (p);
  return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
  int rval = EXIT_FAILURE;

  // This is the actual test
  if (argc == 2)
    rval = read_test (argv[1]);
  else
    // Use this (once on x86 and once on PPC) to make the files for the test
    if (argc == 3 && strcmp (argv[1], "--write") == 0)
      rval = write_protein (argv[2]);
    else
      fprintf (stderr, "Usage: %s binary-protein-file-to-compare\n", argv[0]);

  return rval;
}
