# ecm-tools

Error Code Modeler

Original Author: Neill Corlett

Fork of ECM primarily to host the code because upstream seems dead, with a
minor change to how binaries are called to avoid conflicts with Sage
Mathematics' ECM.

# Usage

##### ECMify

        bin2ecm foo.bin
        bin2ecm foo.bin bar.bin.ecm

##### UnECMify

        ecm2bin foo.bin.ecm
        ecm2bin foo.bin.ecm bar.bin
