[NAME]
pthash \- generate a minimal perfect hash function

[DESCRIPTION]

.IX Header "DESCRIPTION"
PTHash is a C++ library implementing fast and compact minimal perfect hash functions
.PP
.IP \[bu] 2
Minimal and Non\-Minimal Perfect Hash Functions
.IP \[bu] 2
Space/Time Efficiency: fast lookup within compressed space
.IP \[bu] 2
External\-Memory Scaling
.IP \[bu] 2
Multi\-Threaded Construction
.IP \[bu] 2
Configurable: can offer different trade\-offs (between construction
time, lookup time, and space effectiveness)
.PP
Given a set \fIS\fR of \fIn\fR distinct keys, a function \fIf\fR that bijectively maps the keys of \fIS\fR into the first \fIn\fR natural numbers is called a \fIminimal perfect hash function\fR (MPHF) for \fIS\fR.
Algorithms that find such functions when \fIn\fR is large and retain constant evaluation time are of practical interest.
For instance, search engines and databases typically use minimal perfect hash functions to quickly assign identifiers to static sets of variable\-length keys such as strings.
The challenge is to design an algorithm which is efficient in three different aspects: time to find \fIf\fR (construction time), time to evaluate \fIf\fR on a key of \fIS\fR (lookup time), and space of representation for \fIf\fR.
.SS "Enable Multi\-Threading"
.IX Subsection "Enable Multi-Threading"
You can always specify to use multiple threads for construction
with \f(CW\*(C`\-t\*(C'\fR. For example, just append \f(CW\*(C`\-t 4\*(C'\fR to any of the previous build
commands to use 4 parallel threads.
.SS "Building Perfect Hash Functions (not Minimal)"
.IX Subsection "Building Perfect Hash Functions (not Minimal)"
Just do not specify the \f(CW\*(C`\-\-minimal\*(C'\fR flag when using the \f(CW\*(C`pthash\*(C'\fR tool.
.SS "Reading Keys from Standard Input"
.IX Subsection "Reading Keys from Standard Input"
You can make the \f(CW\*(C`pthash\*(C'\fR tool read the keys from stardard input using bash pipelining (\f(CW\*(C`|\*(C'\fR)
in combination with option \f(CW\*(C`\-i \-\*(C'\fR. This is very useful when building keys from compressed files.
.PP
Some examples below.
.PP
.Vb 2
\&        for i in $(seq 1 1000000) ; do echo $i ; done > foo.txt
\&        cat foo.txt | pthash \-\-minimal \-c 5 \-a 0.94 \-e dictionary_dictionary \-n 1000000 \-m 1 \-i \- \-o foo.mph \-\-verbose \-\-external
\&
\&        gzip foo.txt
\&        zcat foo.txt.gz | pthash \-\-minimal \-c 5 \-a 0.94 \-e dictionary_dictionary \-n 1000000 \-m 1 \-i \- \-o foo.mph \-\-verbose \-\-external
\&
\&        gunzip foo.txt.gz
\&        zstd foo.txt
\&        zstdcat foo.txt.zst | pthash \-\-minimal \-c 5 \-a 0.94 \-e dictionary_dictionary \-n 1000000 \-m 1 \-i \- \-o foo.mph \-\-verbose \-\-external
.Ve
.PP
\&\fINote\fR: you may need to write \f(CW\*(C`zcat < foo.txt.gz | (...)\*(C'\fR on Mac OSX.
.PP
One caveat of this approach is that is \fBnot\fR possible to use \f(CW\*(C`\-\-check\*(C'\fR nor \f(CW\*(C`\-\-lookup\*(C'\fR because these two options need to re\-iterate over the keys from the stream.

[EXAMPLES]

.IX Header "EXAMPLES"
.SS "Example 1"
.IX Subsection "Example 1"
.Vb 1
\&        pthash \-n 1000000 \-c 4.5 \-a 0.99 \-e dictionary_dictionary \-s 727369 \-\-minimal \-\-verbose \-\-check \-\-lookup \-o mphf.bin
.Ve
.PP
This example will build a MPHF over 1M random 64\-bit keys (generated with seed 727369), using c = 4.5, alpha = 0.99, and compressing the MPHF data structure with the encoder \`dictionary_dictionary\`.
.PP
The data structure will be serialized on a binary file named \`mphf.bin\`.
With an \`\-o\` argument of \`mphf.hpp\` a readable C++ header file will be created, which can be compiled then.
.PP
It will also check the correctness of the data structure (flag \`\-\-check\`) and measure average lookup time (flag \`\-\-lookup\`).
.PP
Construction will happen in **internal memory**, using a **single processing thread**.
(Experimental setting of the SIGIR paper [1].)
.SS "Example 2"
.IX Subsection "Example 2"
For the following example,
we are going to use the strings from the UK\-2005 URLs collection,
which can be downloaded by clicking
[here](http://data.law.di.unimi.it/webdata/uk\-2005/uk\-2005.urls.gz).
(This is also one of the datasets used in the paper.)
.PP
The file is ~300 MB compressed using gzip (2.86 GB uncompressed).
.PP
After download, place the dataset in the \`build\` directory and run
.PP
.Vb 1
\&        gunzip uk\-2005.urls.gz
.Ve
.PP
to uncompress it.
The file contains one string per line, for a total of 39,459,925 strings.
.PP
#### NOTE: Input files are read line by line (i.e., individual strings are assumed to be separated by the character \`\en\`). Be sure there are no blank lines.
.PP
The following command will build a MPHF using the strings of the file as input keys,
with c = 7.0, alpha = 0.94.
.PP
.Vb 1
\&        pthash \-n 39459925 \-c 7.0 \-a 0.94 \-e dictionary_dictionary \-s 1234567890 \-\-minimal \-i uk\-2005.urls \-\-verbose \-\-check \-\-lookup
.Ve
.SS "Example 3"
.IX Subsection "Example 3"
.Vb 1
\&        pthash \-n 39459925 \-c 7.0 \-a 0.94 \-e dictionary_dictionary \-s 1234567890 \-\-minimal \-i uk\-2005.urls \-\-verbose \-\-check \-\-lookup \-p 128
.Ve
.PP
This example will run the construction over the same input and parameters used in Example 2,
but with 128 **partitions**.
The resulting data structure will consume essentially the same space as that built in Example 2 and only slightly slower at lookup.
.SS "Example 4"
.IX Subsection "Example 4"
.Vb 1
\&        pthash \-n 39459925 \-c 7.0 \-a 0.94 \-e dictionary_dictionary \-s 1234567890 \-i uk\-2005.urls \-\-verbose \-\-check \-\-lookup \-\-external
.Ve
.PP
This example will run the construction over the same input and parameters used in Example 2,
but using **external memory**.
The resulting data structure will be exactly the same as that built in Example 2.

[AUTHOR]

.IP "Giulio Ermanno Pibiri <https://jermp.github.io/>, <giulioermanno.pibiri@unive.it>" 4
.IX Item "Giulio Ermanno Pibiri <https://jermp.github.io/>, <giulioermanno.pibiri@unive.it>"
.PD 0
.IP "Roberto Trani, <roberto.trani@isti.cnr.it>" 4
.IX Item "Roberto Trani, <roberto.trani@isti.cnr.it>"
.IP "Reini Urban <https://github.com/rurban/>, <reini.urban@gmail.com>" 4
.IX Item "Reini Urban <https://github.com/rurban/>, <reini.urban@gmail.com>"
