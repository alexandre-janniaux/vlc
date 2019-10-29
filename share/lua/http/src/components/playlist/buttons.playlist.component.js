Vue.component('playlist-buttons', {
    template: '#button-template',
    computed: {
        ...Vuex.mapState({
            status: state => state.status.data
        }),
    },
    methods: {
        toggleRepeat() {
            this.$store.dispatch('status/toggleRepeat');
        },
        startPlaylist() {
            this.$store.dispatch('status/play', this.status.currentplid);
        },
        toggleRandom() {
            this.$store.dispatch('status/toggleRandom');
        }
    }
});
