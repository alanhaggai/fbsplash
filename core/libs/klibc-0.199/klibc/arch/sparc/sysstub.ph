# -*- perl -*-
#
# arch/sparc32/sysstub.ph
#
# Script to generate system call stubs
#

sub make_sysstub($$$$@) {
    my($fname, $type, $sname, $stype, @args) = @_;

    $stype = $stype || 'common';

    open(OUT, '>', "syscalls/${fname}.S");
    print OUT "#include <asm/unistd.h>\n";
    print OUT "\n";
    print OUT "\t.type ${fname},\@function\n";
    print OUT "\t.globl ${fname}\n";
    print OUT "${fname}:\n";
    print OUT "\tb __syscall_${stype}\n";
    print OUT "\t  mov\t__NR_${sname}, %g1\n";
    print OUT "\t.size ${fname},.-${fname}\n";
    close(OUT);
}

1;
