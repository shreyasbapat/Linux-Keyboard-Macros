from tkinter import *
from functools import partial

macros = []
with open('/usr/include/linux/input-event-codes.h', 'r') as f:
    keys = f.readlines()

    only_keys = []
    for key in keys:
        if 'KEY_' in key:
            only_keys.append(key.strip().split())
    key_map = {}
    key_revmap = {}
    for key in only_keys:
        if(len(key) == 3):
            key_map[key[-1]] = key[-2]
            key_revmap[key[-2]] = key[-1]


with open('/proc/keymac_proc', 'r') as f:
    Macros = f.readlines()
    macros = []
    for macro in Macros:
        key_comb, macro = macro.strip().split(':')
        key_comb = key_comb.strip().split(' ')
        macro = macro.strip().split()
        lhs = []
        rhs = []
        for key in key_comb:
            lhs.append(key_map[key])
        for mac in macro:
            rhs.append(key_map[mac])
        macros.append([lhs, rhs])

macro_id = []
key_sequence = []


def store_macros():
    with open('macros.txt', 'w') as f:
        line = ''
        for lhs, rhs in macros:
            for key in lhs:
                line += key_revmap[key] + ' '
            line += ': '
            for key in rhs:
                line += key_revmap[key] + ' '
            line = line[:-1]
            line += '\n'
        line = line[:-1]
        print(line)
        f.write(line)


def reload_macros():
    global macros
    with open('macros.txt', 'r') as f:
        Macros = f.readlines()
        macros = []
        for macro in Macros:
            key_comb, macro = macro.strip().split(':')
            key_comb = key_comb.strip().split(' ')
            macro = macro.strip().split()
            lhs = []
            rhs = []
            for key in key_comb:
                lhs.append(key_map[key])
            for mac in macro:
                rhs.append(key_map[mac])
            macros.append([lhs, rhs])
    write_macros()


def write_macros():
    with open('/proc/keymac_proc', 'w') as f:
        line = ''
        for lhs, rhs in macros:
            for key in lhs:
                line += key_revmap[key] + ' '
            line += ': '
            for key in rhs:
                line += key_revmap[key] + ' '
            line = line[:-1]
            line += '\n'
        line = line[:-1]
        print(line)
        f.write(line)


def save_macros():
    global error
    keys = []
    for key in key_sequence:
        keys.append(key.get())
        if key.get() not in key_revmap.keys():
            error.set(key.get() + " is not a valid key")
            return

    for key in keys:
        if keys.count(key) % 2:
            location = max(loc for loc, val in enumerate(keys) if val == key)
            error.set(key + "not released at location " + str(location))
            return

    for i in range(len(macros)):
        if macros[i][0] == macro_id:
            for j in range(len(macros[i][1])):
                macros[i][1][j] = keys[j]
    error.set("Key Combination Saved successfully")
    write_macros()
    return


def delete_macros():
    global error
    for i in range(len(macros)):
        if macros[i][0] == macro_id:
            macros.pop(i)
            start()
            error.set("Deleted macros successfully")
            for widget in right.winfo_children():
                widget.destroy()
            write_macros()
            return


def makeform(right, macro, flag):
    global error, macro_id, key_sequence
    for widget in right.winfo_children():
        widget.destroy()
    if flag == 'delete_cells' and len(macro[1]) >= 2:
        macro[1].pop()
        macro[1].pop()
        error.set('Cells Deleted Successfully')
    if flag == 'add_cells':
        macro[1].append('KEY_A')
        macro[1].append('KEY_A')
        error.set('Cells Added Successfully')

    lhs = macro[0]
    rhs = macro[1]
    macro_id = lhs[:]
    key_sequence = []
    right.grid(row=0, column=1)
    label = lhs[0] + ' ' + lhs[1] + ' ' + lhs[2]
    lab = Label(right, text=label, anchor='w')
    lab.grid(row=1, columnspan=4)
    c = 0
    r = 2
    for data in rhs:
        v = StringVar()
        ent = Entry(right, textvariable=v)
        ent.grid(row=r, column=c)
        v.set(data)
        key_sequence.append(v)
        c += 1
        if c == 4:
            r += 1
            c = 0
    while r < 11:
        rb = Label(right,
                   text='')
        rb.grid(row=r, column=c, sticky='ewns')
        c += 1
        if c == 4:
            r += 1
            c = 0
    add_cells = Button(right, text="Add Cells", fg="red", command=partial(makeform, right, [lhs, rhs], 'add_cells'))
    add_cells.grid(row=r + 1, column=0)
    delete_cells = Button(right, text="Delete Cells", fg="red", command=partial(makeform, right, [lhs, rhs], 'delete_cells'))
    delete_cells.grid(row=r + 1, column=1)

    error.set("Go Ahead!")
    save_button = Button(right, text="Save", fg="red", command=save_macros)
    save_button.grid(row=r + 2, column=0)

    delete_button = Button(right, text="Delete", fg="red", command=delete_macros)
    delete_button.grid(row=r + 2, column=1)

    error_label = Label(right, textvariable=error)
    error_label.grid(row=r + 2, column=2, columnspan=2)


def start():
    r = 1
    for lhs, rhs in macros:
        rb = Button(left, text=lhs[0] + ' ' + lhs[1] + ' ' + lhs[2], padx=20, command=partial(makeform, right, [lhs, rhs], 'nothing'))
        rb.grid(row=r + 1, column=0)
        r += 1
    while r < 11:
        rb = Label(left, text='')
        rb.grid(row=r + 1, column=0, sticky='ewns')
        r += 1
    store = Button(left, text='Store Macros', padx=20, command=store_macros)
    store.grid(row=r + 1, column=0)
    reload = Button(left, text='Reload Macros', padx=20, command=reload_macros)
    reload.grid(row=r + 1, column=1)


root = Tk()
error = StringVar()
left = Frame(root)
right = Frame(root)
left.grid(row=0, column=0)
choose = Label(left, text="""Choose the Macros to be edited:""", justify=LEFT, padx=20)
choose.grid(row=0, column=0)

start()
root.mainloop()
