export default {
    postCmd(cmd) {
        return $.ajax({
            url: 'requests/vlm_cmd.xml',
            data: `command=${cmd}`
        });
    }
};
