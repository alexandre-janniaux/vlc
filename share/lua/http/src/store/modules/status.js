import statusService from '../../services/status.service.js';

// initial state
const state = {
    data: {}
};

const getters = {};

const actions = {
    fetchStatus({ commit }) {
        statusService.fetchStatus()
            .then((status) => {
                commit('setStatus', status);
            });
    },
    toggleRandom() {
        statusService.toggleRandom();
    },
    toggleRepeat() {
        statusService.toggleRepeat();
    },
    play({}, id) {
        statusService.play(id);
    },
    pause() {
        statusService.pause();
    },
    updateVolume({}, volume) {
        statusService.updateVolume(volume);
    }
};

const mutations = {
    setStatus(state, status) {
        state.data = status;
    }
};

export default {
    namespaced: true,
    state,
    getters,
    actions,
    mutations
};
