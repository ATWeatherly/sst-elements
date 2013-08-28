#!/usr/bin/perl -w

$n = $ARGV[0];

if(scalar @ARGV == 5) {
    $FST = 'none'; 
}
else {
    $FST = $ARGV[5];
}

{
    print <<EOT
<?xml version="1.0"?>
<sdl version="2.0"/>

<config>
    run-mode=both
    partitioner=self
</config>

<sst>
  <component name="s" type="scheduler.schedComponent" rank=0>
    <params>
      <traceName>$ARGV[1]</traceName>
	<scheduler>$ARGV[2]</scheduler>
	<machine>$ARGV[3]</machine>
	<allocator>$ARGV[4]</allocator>
        <FST>$FST</FST>
    </params>
EOT
}

for ($i = 0; $i < $n; ++$i) {
    printf("   <link name=\"l$i\" port=\"nodeLink$i\" latency=\"0 ns\"/>\n");
}
printf(" </component>\n\n");

for ($i = 0; $i < $n; ++$i) {
print <<EOT
 <component name="n$i" type="scheduler.nodeComponent" rank=0>
   <params>
     <nodeNum>$i</nodeNum>
   </params>
   <link name="l$i" port="Scheduler" latency="0 ns"/>
 </component>
EOT
}

printf("</sst>\n");
