// initial state
const state = {
    mainView: 'player'
};

const getters = {};

const actions = {
    setLibraryView({ commit }) {
        commit('setMainView', 'library');
    },
    setPlayerView({ commit }) {
        commit('setMainView', 'player');
    }
};

const mutations = {
    setMainView(state, value) {
        state.mainView = value;
    }
};

export default {
    namespaced: true,
    state,
    getters,
    actions,
    mutations
};
