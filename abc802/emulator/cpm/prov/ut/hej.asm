; HEJ.ASM -- provet till ASM och LOAD (cpm/prov.sh), i 8080:s kod
        ORG     100H
        MVI     C,9
        LXI     D,TEXT
        CALL    5
        RET
TEXT:   DB      'HEJ FR}N ABC 802!$'
        END
