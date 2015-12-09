global on_lack_of_page

extern process_lack_page

on_lack_of_page:
    call process_lack_page
    add esp, 4
    iret
