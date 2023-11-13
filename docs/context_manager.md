Enhance Context Management for Domain
===================================================


Introduction
------------

The context management component in OpenSBI provides the basic CPU context
initialization and management routines based on the existing domain in OpenSBI. 
The initial design of this context managemnt component is to realzie the secure 
and the non-secure domain context switch which is used by the UEFI Secure Variable service.


Context Allocation and Initialization
-------------------------------------

The below figure shows how the CPU context is allocated and managed within OpenSBI. 

![image](https://github.com/Penglai-Enclave/opensbi/assets/21300636/f16cb357-6bb1-4143-b3cd-b49135c3d928)

The secure domain contexts are configured in device tree and initialized by the root context 
after the domain has been configured and the SBI interface is ready. Then the non secure context then can hook an opensbi message to the context manager, 
and the context manager will save the non-secure context and switch to execute the context in secure domain.
Once the context in secure domain has been executed, it will return to the non-secrure context with result.
The current sequence for context save and restore in OpenSBI is as given below:

![image](https://github.com/Penglai-Enclave/opensbi/assets/21300636/0a9cbba2-06e4-4453-99ca-585ddff925ea)

Conclusion
----------
