Vue.component('library', {
    template: '#library-template',
    computed: {
        ...Vuex.mapState({
            mainView: state => state.layout.mainView,
        })
    },
    methods: {
        setLibraryView() {
            this.$store.dispatch('layout/setLibraryView');
        }
    }
});
