import vlmCmdService from '../../services/vlm_cmd.service.js';

// initial state
const state = {
    data: {}
};

const getters = {};

const actions = {
    postCmd({}, cmd) {
        vlmCmdService.postCmd(cmd);
    }
};

const mutations = {

};

export default {
    namespaced: true,
    state,
    getters,
    actions,
    mutations
};
