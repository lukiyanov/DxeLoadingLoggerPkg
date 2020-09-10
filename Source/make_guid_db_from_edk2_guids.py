import os

import guids.ami_guids
import guids.asrock_guids
import guids.dell_guids
import guids.lenovo_guids
import guids.edk2_guids


def global_var_from_guid(guid_name):
    # ACOUSTIC_SETUP_PROTOCOL_GUID -> gAcousticSetupProtocolGuid

    if guid_name.startswith('g'):
        return guid_name

    global_var_name = 'g'
    next_capital = True
    for char in guid_name:
        if char == '_':
            next_capital = True
            continue
        if next_capital:
            global_var_name += char.upper()
            next_capital = False
        else:
            global_var_name += char.lower()

    return global_var_name


def guid_from_global_var(var):
    # gAcousticSetupProtocolGuid -> ACOUSTIC_SETUP_PROTOCOL_GUID

    if not var.startswith('g'):
        return var

    guid_name = ''
    first_g = True
    for char in var:
        if first_g:
            first_g = False
            continue

        if char.isupper():
            guid_name += '_'
            guid_name += char.upper()
        else:
            guid_name += char.upper()

    return guid_name.lstrip('_')


def add_guids(add_to, guids):
    for key in guids:
        guid_key   = guid_from_global_var(key)
        guid_value = guids[key]
        if (guid_key not in add_to) and (guid_value not in add_to.values()):
            add_to[guid_key] = guid_value

    return add_to


def generate_guid_db_file():
    guids_all = {}
    add_guids(guids_all, guids.edk2_guids.edk2_guids)
    add_guids(guids_all, guids.ami_guids.ami_guids)
    add_guids(guids_all, guids.asrock_guids.asrock_guids)
    add_guids(guids_all, guids.dell_guids.dell_guids)
    add_guids(guids_all, guids.lenovo_guids.lenovo_guids)

    with open('guid_db.h', 'w') as guid_header:
        guid_header.write('#include <Uefi.h>\n\n')

        for guid_name in guids_all:
            guid_value = guids_all[guid_name]
            guid_header.write('static EFI_GUID my_{} = {{\n'.format(global_var_from_guid(guid_name)))
            guid_header.write('  {}, {}, {}, {{ {}, {}, {}, {}, {}, {}, {}, {} }}\n'.format(
                hex(guid_value[0]),
                hex(guid_value[1]),
                hex(guid_value[2]),
                hex(guid_value[3]),
                hex(guid_value[4]),
                hex(guid_value[5]),
                hex(guid_value[6]),
                hex(guid_value[7]),
                hex(guid_value[8]),
                hex(guid_value[9]),
                hex(guid_value[10])
            ))
            guid_header.write('};\n\n')

        guid_header.write('#define GUID_DB \\\n')
        guid_db_def = ''
        for guid_name in guids_all:
            guid_db_def += '  {{ &my_{}, L"{}" }}, \\\n'.format(
                global_var_from_guid(guid_name),
                guid_from_global_var(guid_name)
            )
        guid_db_def = guid_db_def.rstrip(', \\\n')
        guid_db_def += '\n'
        guid_header.write(guid_db_def)


generate_guid_db_file()
