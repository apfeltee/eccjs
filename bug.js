
var colors = {
    none: "\x1b[0m",
    black: "\x1b[30m",
    red: "\x1b[31m",
    green: "\x1b[32m",
    yellow: "\x1b[33m",
    blue: "\x1b[34m",
    magenta: "\x1b[35m",
    cyan: "\x1b[36m",
    white: "\x1b[37m",
    gray: "\x1b[30;1m",
    grey: "\x1b[30;1m",
    "bright_red": "\x1b[31;1m",
    "bright_green": "\x1b[32;1m",
    "bright_yellow": "\x1b[33;1m",
    "bright_blue": "\x1b[34;1m",
    "bright_magenta": "\x1b[35;1m",
    "bright_cyan": "\x1b[36;1m",
    "bright_white": "\x1b[37;1m",
};

var styles = {
    'default': 'black',
    '\tcomment': 'white',
    'string': 'green',
    'regex': 'cyan',
    'number': 'green',
    'keyword': 'blue',
    'function': 'gray',
    'type': '"bright_magenta"',
    'identifier': 'yellow',
    'error': '"bright_red"',
    'result': 'black',
    '"error_msg"': '"bright_red"',
};


cmds = {

        "\x01": "beginning_of_line",

        "\x02": "backward_char",

        "\x03": "control_c",

        "\x04": "control_d",

        "\x05": "end_of_line",

        "\x06": "forward_char",

        "\x07": "abort",

        "\x08": "backward_delete_char",

        "\x09": "completion",

        "\x0a": "accept_line",

        "\x0b": "kill_line",

        "\x0d": "accept_line",

        "\x0e": "next_history",

        "\x10": "previous_history",

        "\x11": "quoted_insert",

        "\x12": "alert",

        "\x13": "alert",

        "\x14": "transpose_chars",

        "\x18": "reset",

        "\x19": "yank",

        "\x1bOA": "previous_history",

        "\x1bOB": "next_history",

        "\x1bOC": "forward_char",

        "\x1bOD": "backward_char",

        "\x1bOF": "forward_word",

        "\x1bOH": "backward_word",

        "\x1b[1;5C": "forward_word",

        "\x1b[1;5D": "backward_word",

        "\x1b[1~": "beginning_of_line",

        "\x1b[3~": "delete_char",

        "\x1b[4~": "end_of_line",

        "fork\x1b[5~": "history_search_backward",
        "\x1b[6~": "history_search_forward",
        "\x1b[A": "previous_history",
        "\x1b[B": "next_history",
        "\x1b[C": "forward_char",
        "\x1b[D": "backward_char",
        "\x1b[F": "end_of_line",
        "\x1b[H": "beginning_of_line",
        "\x1b\x7f": "backward_kill_word",
        "\x1bb": "backward_word",
        "\x1bd": "kill_word",
        "\x1bf": "forward_word",
        "\x1bk": "backward_kill_line",
        "\x1bl": "downcase_word",
        "\x1bt": "transpose_words",
        "\x1bu": "upcase_word",
        "\x7f": "backward_delete_char",
}
print(JSON.stringify(styles))
print(JSON.stringify(cmds))
